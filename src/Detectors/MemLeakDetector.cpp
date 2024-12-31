//
// Created by Xiao on 2022/1/2.
//

#include "Detectors/MemLeakDetector.h"
#include "PSTA/PSAStat.h"
#include "PSTA/config.h"
#include "PSTA/Logger.h"

using namespace SVF;
using namespace SVFUtil;

void MemLeakDetector::initHandler(SVFModule *module) {
    PSTA::initHandler(module);
    _stat->absTranserFuncStart();
    getAbsTransitionHandler()->initAbsTransitionFuncs(_curEvalSVFGNode, _curEvalFuns, true);
    _stat->absTranserFuncEnd();
    Set<FSMParser::CHECKER_TYPE> checkerTypes = {FSMParser::CHECKER_TYPE::CK_RET};
    _symStateMgr.setCheckerTypes(checkerTypes);
    getAbsTransitionHandler()->initSnks(_curEvalSVFGNode, _snks, checkerTypes);
    if (!PSAOptions::MultiSlicing()) {
        if (PSAOptions::PathSensitive())
            _graphSparsificator.normalSlicing(_curEvalSVFGNode, _snks);
    } else {
        if (PSAOptions::PathSensitive())
            _graphSparsificator.multipointSlicing(module, checkerTypes, _curEvalSVFGNode, _snks, _mainEntry);
    }
}

bool MemLeakDetector::runFSMOnModule(SVFModule *module) {
    /// start analysis
    _fsmFile = PROJECT_SOURCE_ROOT;
    if (PSAOptions::Wrapper()) {
        _fsmFile += "/res/memleak.fsm";
    } else {
        _fsmFile += "/res/rw/memleak.fsm";
    }
    analyze(module);
    performStat("leak");
    return false;
}

void MemLeakDetector::reportNeverFree(const Z3Expr &branchCond, const KeyNodesSet &keyNodesSet) {
    _stat->incBugNum();
    Log(LogLevel::Error) << SVFUtil::bugMsg1("\t NeverFree :") << " memory allocation at : ("
                    << _curEvalSVFGNode->getValue()->getSourceLoc() << ")\n";
    Dump() << SVFUtil::bugMsg1("\t NeverFree :") << " memory allocation at : ("
           << _curEvalSVFGNode->getValue()->getSourceLoc() << ")\n";
    if (PSAOptions::EnableReport())
        detailBugReport(branchCond, keyNodesSet);
}

void MemLeakDetector::reportPartialLeak(const Z3Expr &branchCond, const KeyNodesSet &keyNodesSet) {
    _stat->incBugNum();
    Log(LogLevel::Error) << SVFUtil::bugMsg2("\t PartialLeak :") << " memory allocation at : ("
                    << _curEvalSVFGNode->getValue()->getSourceLoc() << ")\n";
    Dump() << SVFUtil::bugMsg2("\t PartialLeak :") << " memory allocation at : ("
           << _curEvalSVFGNode->getValue()->getSourceLoc() << ")\n";
    if (PSAOptions::EnableReport())
        detailBugReport(branchCond, keyNodesSet);
}

void MemLeakDetector::reportBug() {
    ICFGNodeWrapper *srcWrapper = ICFGWrapper::getICFGWrapper(_icfg)->getICFGNodeWrapper(_curEvalICFGNode->getId());
    // src is deleted because no PI is found
    if (!srcWrapper || srcWrapper->getInEdges().empty()) return;
    bool srcReachable = false;
    ConsExeState srcReachableExeState(ExeStateManager::nullExeState());
    for (const auto &e: srcWrapper->getInEdges()) {
        InfoKey srcInfoKey = std::make_pair(e, getFSMParser()->getUninitAbsState());
        auto srcReachableIt = _infoMap.find(srcInfoKey);
        if (srcReachableIt != _infoMap.end()) {
            for (const auto &s: srcReachableIt->second) {
                if (s.first == getFSMParser()->getUninitAbsState()) {
                    if (srcReachableExeState.isNullState())
                        srcReachableExeState = s.second.getExecutionState();
                    else
                        srcReachableExeState.joinWith(s.second.getExecutionState());
                    break;
                }
            }
            srcReachable = true;
        }
    }
    // src unreachable
    if (!srcReachable) {
        return;
    }
    if (srcReachableExeState.isNullState()) {
        return;
    }

    AbsToSymState absToSymState;
    bool hasError = false, hasUninit = false;
    Z3Expr brc;
    KeyNodesSet keyNodesSet;
    PSTA::SummaryKey uninitKey = std::make_pair(_mainFunc, getFSMParser()->getUninitAbsState());
    auto uninitIt = PSTA::_summaryMap.find(uninitKey);
    if (uninitIt == PSTA::_summaryMap.end()) return;
    absToSymState = SVFUtil::move(uninitIt->second);
    for (const auto &s: absToSymState) {
        if (!getAbsTransitionHandler()->reachGlobal(_curEvalSVFGNode) &&
            s.first == getFSMParser()->getErrAbsState()) {
            hasError = true;
            brc = s.second.getBranchCondition();
            keyNodesSet = SVFUtil::move(s.second.getKeyNodesSet());
        }
        if (s.first == TypeState::Freed) {
            hasUninit = true;
        }
    }
    LEAK_TYPE leakType = SAFE;
    // src not reach return, maybe dead loop
    if (!hasError && !hasUninit) {
        return;
    }
    if (hasError) {
        if (!hasUninit) {
            reportNeverFree(brc, keyNodesSet);
            leakType = NEVER_FREE_LEAK;
        } else {
            reportPartialLeak(brc, keyNodesSet);
            leakType = PATH_LEAK;
        }

    }
    if (PSAOptions::ValidateTests()) {
        validateSuccessTests(leakType);
        validateExpectedFailureTests(leakType);
    }


}

void MemLeakDetector::validateSuccessTests(LEAK_TYPE leakType) {
    const CallICFGNode *callBlockNode = dyn_cast<CallICFGNode>(_curEvalICFGNode);
    if (!callBlockNode) return;
    Set<const SVFFunction *> functionSet;
    _ptaCallgraph->getCallees(callBlockNode, functionSet);
    const SVFFunction *fun = *functionSet.begin();
    if (!fun) return;
    std::string name = fun->getName();

    bool success = false;

    if (name == "SAFEMALLOC") {
        if (leakType == SAFE)
            success = true;
    } else if (name == "NFRMALLOC") {
        if (leakType == NEVER_FREE_LEAK)
            success = true;
    } else if (name == "PLKMALLOC") {
        if (leakType == PATH_LEAK)
            success = true;
    } else if (name == "CLKMALLOC") {
        if (leakType == NEVER_FREE_LEAK)
            success = true;
    } else if (name == "NFRLEAKFP" || name == "PLKLEAKFP"
               || name == "LEAKFN") {
        return;
    } else {
        writeWrnMsg("\t can not validate, check function not found, please put it at the right place!!");
        return;
    }

    if (success)
        outs() << SVFUtil::sucMsg("\t SUCCESS :") << name << " check <src id:" << _curEvalICFGNode->getId() << "> at ("
               << callBlockNode->getCallSite()->getSourceLoc() << ")\n";
    else {
        SVFUtil::errs() << SVFUtil::errMsg("\t FAILURE :") << name << " check <src id:" << _curEvalICFGNode->getId()
                        << "> at ("
                        << callBlockNode->getCallSite()->getSourceLoc() << ")\n";
        assert(false && "test case failed!");
    }
}

void MemLeakDetector::validateExpectedFailureTests(LEAK_TYPE leakType) {
    const CallICFGNode *callBlockNode = dyn_cast<CallICFGNode>(_curEvalICFGNode);
    if (!callBlockNode) return;
    Set<const SVFFunction *> functionSet;
    _ptaCallgraph->getCallees(callBlockNode, functionSet);
    const SVFFunction *fun = *functionSet.begin();
    if (!fun) return;
    std::string name = fun->getName();
    bool expectedFailure = false;

    if (name == "NFRLEAKFP") {
        if (leakType == NEVER_FREE_LEAK)
            expectedFailure = true;
    } else if (name == "PLKLEAKFP") {
        if (leakType == PATH_LEAK)
            expectedFailure = true;
    } else if (name == "LEAKFN") {
        if (leakType == SAFE)
            expectedFailure = true;
    } else if (name == "SAFEMALLOC" || name == "NFRMALLOC"
               || name == "PLKMALLOC" || name == "CLKLEAKFN") {
        return;
    } else {
        writeWrnMsg("\t can not validate, check function not found, please put it at the right place!!");
        return;
    }


    if (expectedFailure)
        outs() << SVFUtil::sucMsg("\t EXPECTED-FAILURE :") << name << " check <src id:" << _curEvalICFGNode->getId()
               << "> at ("
               << callBlockNode->getCallSite()->getSourceLoc() << ")\n";
    else {
        SVFUtil::errs() << SVFUtil::errMsg("\t UNEXPECTED FAILURE :") << name
                        << " check <src id:" << _curEvalICFGNode->getId() << "> at ("
                        << callBlockNode->getCallSite()->getSourceLoc() << ")\n";
        assert(false && "test case failed!");
    }
}