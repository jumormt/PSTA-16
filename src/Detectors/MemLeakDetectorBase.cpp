//
// Created by Jiawei Ren on 2023/7/2.
//

#include "Detectors/MemLeakDetectorBase.h"
#include "PSTA/config.h"
#include "PSTA/Logger.h"

using namespace SVF;
using namespace SVFUtil;

void MemLeakDetectorBase::initHandler(SVFModule *module) {
    PSTABase::initHandler(module);
    getAbsTransitionHandler()->initAbsTransitionFuncs(_curEvalSVFGNode, _curEvalFuns, true);
    Set<FSMParser::CHECKER_TYPE> checkerTypes = {FSMParser::CHECKER_TYPE::CK_RET};
    _symStateMgr.setCheckerTypes(checkerTypes);
}

bool MemLeakDetectorBase::runFSMOnModule(SVFModule *module) {
    /// start analysis
    _fsmFile = PROJECT_SOURCE_ROOT;
    if (PSAOptions::Wrapper()) {
        _fsmFile += "/res/memleak.fsm";
    } else {
        _fsmFile += "/res/rw/memleak.fsm";
    }
    analyze(module);
    return false;
}

void MemLeakDetectorBase::reportNeverFree(const Z3Expr &branchCond, const KeyNodesSet &keyNodesSet) {
    Log(LogLevel::Error) << SVFUtil::bugMsg1("\t NeverFree :") << " memory allocation at : ("
                         << _curEvalSVFGNode->getValue()->getSourceLoc() << ")\n";
    Dump() << SVFUtil::bugMsg1("\t NeverFree :") << " memory allocation at : ("
           << _curEvalSVFGNode->getValue()->getSourceLoc() << ")\n";
}

void MemLeakDetectorBase::reportPartialLeak(const Z3Expr &branchCond, const KeyNodesSet &keyNodesSet) {
    Log(LogLevel::Error) << SVFUtil::bugMsg2("\t PartialLeak :") << " memory allocation at : ("
                         << _curEvalSVFGNode->getValue()->getSourceLoc() << ")\n";
    Dump() << SVFUtil::bugMsg2("\t PartialLeak :") << " memory allocation at : ("
           << _curEvalSVFGNode->getValue()->getSourceLoc() << ")\n";
}

void MemLeakDetectorBase::reportBug() {
    ICFGNode *src = _icfg->getICFGNode(_curEvalICFGNode->getId());
    // src is deleted because no PI is found
    if (!src || src->getInEdges().empty()) return;
    bool srcReachable = false;
    ConsExeState srcReachableExeState(ExeStateManager::nullExeState());
    for (const auto &e: src->getInEdges()) {
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
    PSTABase::SummaryKey uninitKey = std::make_pair(_mainFunc, getFSMParser()->getUninitAbsState());
    auto uninitIt = PSTABase::_summaryMap.find(uninitKey);
    if (uninitIt == PSTABase::_summaryMap.end()) return;
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

void MemLeakDetectorBase::validateSuccessTests(LEAK_TYPE leakType) {
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

void MemLeakDetectorBase::validateExpectedFailureTests(LEAK_TYPE leakType) {
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