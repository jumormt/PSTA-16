//
// Created by Jiawei Ren on 2023/7/2.
//

#include "Detectors/DFDetectorBase.h"
#include "PSTA/PSAOptions.h"
#include "PSTA/config.h"
#include "PSTA/Logger.h"


using namespace SVF;
using namespace SVFUtil;

void DFDetectorBase::initHandler(SVFModule *module) {
    PSTABase::initHandler(module);
    getAbsTransitionHandler()->initAbsTransitionFuncs(_curEvalSVFGNode, _curEvalFuns, false);
    Set<FSMParser::CHECKER_TYPE> checkerTypes;
    if (!PSAOptions::Wrapper())
        checkerTypes = {FSMParser::CK_FREE};
    else
        checkerTypes = {FSMParser::CK_FREE_WRAPPER};
    _symStateMgr.setCheckerTypes(checkerTypes);
    getAbsTransitionHandler()->initSnks(_curEvalSVFGNode, _snks, checkerTypes);
}

bool DFDetectorBase::runFSMOnModule(SVFModule *module) {
    /// start analysis
    _fsmFile = PROJECT_SOURCE_ROOT;
    if (PSAOptions::Wrapper()) {
        _fsmFile += "/res/df.fsm";
    } else {
        _fsmFile += "/res/rw/df.fsm";
    }
    analyze(module);
    return false;
}

// update for double free
void DFDetectorBase::reportDF(const ICFGNode *node) {
    Log(LogLevel::Error) << SVFUtil::bugMsg1("\t DOUBLE_FREE :") << " memory allocation at : ("
                         << _curEvalSVFGNode->getValue()->getSourceLoc()
                         << " double free location at: ("
                         << node->toString() << ")\n";
    Dump() << SVFUtil::bugMsg1("\t DOUBLE_FREE :") << " memory allocation at : ("
           << _curEvalSVFGNode->getValue()->getSourceLoc()
           << " double free location at: ("
           << node->toString() << ")\n";
}


void DFDetectorBase::reportBug() {
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
    if (!srcReachable) return;
    if (srcReachableExeState.isNullState()) return;
    bool hasError = false;
    Set<const ICFGNode *> errNodes;
    Set<const ICFGNode *> reachableNodes;
    for (const auto &node: _snks) {
        const ICFGNode *evalNode = _icfg->getICFGNode(node->getId());
        if (PSAOptions::Wrapper())
            evalNode = _icfg->getICFGNode(
                    dyn_cast<CallICFGNode>(node)->getRetICFGNode()->getId());
        else if (SVFUtil::isa<CallICFGNode>(evalNode)) {
            evalNode = _icfg->getICFGNode(
                    dyn_cast<CallICFGNode>(node)->getRetICFGNode()->getId());
        }
        // snk not reachable
        if (!evalNode) continue;
        for (const auto &t: getOutTEdges(evalNode)) {
            for (const auto &absState: getFSMParser()->getAbsStates()) {
                InfoKey tmpInfoKey = std::make_pair(
                        t,
                        absState);
                auto it = _infoMap.find(tmpInfoKey);
                if (it != _infoMap.end()) {
                    auto symStates = it->second;
                    for (const auto &s: symStates) {
                        if (s.second.isNullSymState() ||
                            !isSrcSnkReachable(srcReachableExeState, s.second.getExecutionState()))
                            continue;
                        reachableNodes.insert(node);
                        if (s.first == getFSMParser()->getErrAbsState()) {
                            errNodes.insert(node);
                            hasError = true;
                            break;
                        }
                    }
                }
            }
        }
    }
    for (const auto &node: errNodes) {
        reportDF(node);
    }
    if (PSAOptions::ValidateTests()) {
        for (const auto &node: _snks) {
            if (reachableNodes.count(node)) {
                if (errNodes.count(node))
                    validateSuccessTests(DF_ERROR, node);
                else
                    validateSuccessTests(SAFE, node);
            }
        }
        for (const auto &node: errNodes) {
            if (reachableNodes.count(node)) {
                if (errNodes.count(node))
                    validateExpectedFailureTests(DF_ERROR, node);
                else
                    validateExpectedFailureTests(SAFE, node);
            }
        }
    }
}

void DFDetectorBase::validateSuccessTests(DF_TYPE dfType, const ICFGNode *node) {
    const CallICFGNode *callBlockNode = dyn_cast<CallICFGNode>(_curEvalICFGNode);
    if (!callBlockNode) return;
    Set<const SVFFunction *> functionSet;
    _ptaCallgraph->getCallees(callBlockNode, functionSet);
    const SVFFunction *fun = *functionSet.begin();
    if (!fun) return;
    functionSet.clear();
    _ptaCallgraph->getCallees(SVFUtil::dyn_cast<CallICFGNode>(node), functionSet);
    std::string name = (*functionSet.begin())->getName();

    bool success = false;

    if (name == "SAFEFREE") {
        if (dfType == SAFE)
            success = true;
    } else if (name == "DOUBLEFREE") {
        if (dfType == DF_ERROR)
            success = true;
    } else {
        writeWrnMsg("\t can not validate, check function not found, please put it at the right place!!");
        return;
    }
    if (success)
        outs() << SVFUtil::sucMsg("\t SUCCESS :") << name << " check <src id:" << _curEvalICFGNode->getId() << "> at ("
               << callBlockNode->getCallSite()->getSourceLoc() << " free location at: ("
               << dyn_cast<CallICFGNode>(node)->getCallSite()->getSourceLoc() << ")\n";
    else {
        SVFUtil::errs() << SVFUtil::errMsg("\t FAILURE :") << name << " check <src id:" << _curEvalICFGNode->getId()
                        << callBlockNode->getCallSite()->getSourceLoc() << " free location at: ("
                        << dyn_cast<CallICFGNode>(node)->getCallSite()->getSourceLoc() << ")\n";
        assert(false && "test case failed!");
    }
}

void DFDetectorBase::validateExpectedFailureTests(DF_TYPE dfType, const ICFGNode *node) {
    const CallICFGNode *callBlockNode = dyn_cast<CallICFGNode>(_curEvalICFGNode);
    if (!callBlockNode) return;
    Set<const SVFFunction *> functionSet;
    _ptaCallgraph->getCallees(callBlockNode, functionSet);
    const SVFFunction *fun = *functionSet.begin();
    if (!fun) return;
    functionSet.clear();
    _ptaCallgraph->getCallees(SVFUtil::dyn_cast<CallICFGNode>(node), functionSet);
    std::string name = (*functionSet.begin())->getName();
    bool expectedFailure = false;

    if (name == "DOUBLEFREE_FP") {
        if (dfType == DF_ERROR)
            expectedFailure = true;
    } else if (name == "SAFEFREE") {
        return;
    } else {
        writeWrnMsg("\t can not validate, check function not found, please put it at the right place!!");
        return;
    }


    if (expectedFailure)
        outs() << SVFUtil::sucMsg("\t EXPECTED-FAILURE :") << name << " check <src id:" << _curEvalICFGNode->getId()
               << callBlockNode->getCallSite()->getSourceLoc() << " double free location at: ("
               << dyn_cast<CallICFGNode>(node)->getCallSite()->getSourceLoc() << ")\n";
    else {
        SVFUtil::errs() << SVFUtil::errMsg("\t UNEXPECTED FAILURE :") << name
                        << " check <src id:" << _curEvalICFGNode->getId()
                        << callBlockNode->getCallSite()->getSourceLoc() << " double free location at: ("
                        << dyn_cast<CallICFGNode>(node)->getCallSite()->getSourceLoc() << ")\n";
        assert(false && "test case failed!");
    }
}

