//
// Created by Jiawei Ren on 2023/7/2.
//

#include "Detectors/UAFDetectorBase.h"
#include "PSTA/config.h"
#include "PSTA/Logger.h"

using namespace SVF;
using namespace SVFUtil;

void UAFDetectorBase::initHandler(SVFModule *module) {
    PSTABase::initHandler(module);
    getAbsTransitionHandler()->initAbsTransitionFuncs(_curEvalSVFGNode, _curEvalFuns, false);
    Set<FSMParser::CHECKER_TYPE> checkerTypes;
    if (!PSAOptions::Wrapper())
        checkerTypes = {FSMParser::CK_USE};
    else
        checkerTypes = {FSMParser::CK_USE_WRAPPER};
    _symStateMgr.setCheckerTypes(checkerTypes);
    getAbsTransitionHandler()->initSnks(_curEvalSVFGNode, _snks, checkerTypes);
}

bool UAFDetectorBase::runFSMOnModule(SVFModule *module) {
    /// start analysis
    // set FSM file address, store it to PSAOptions::FSMFILE
    _fsmFile = PROJECT_SOURCE_ROOT;
    if (PSAOptions::Wrapper()) { // wrapper is for correctness validation, e.g., SAFEMALLOC, UAFFUNC...
        _fsmFile += "/res/uaf.fsm";//
    } else { // detect bugs from c code without wrapper
        _fsmFile += "/res/rw/uaf.fsm";
    }
    // We init icfg, srcs,
    analyze(module);
    return false;
}

// update for use_after_free
void UAFDetectorBase::reportUAF(const ICFGNode *node, const Z3Expr &branchCond, const KeyNodesSet &keyNodesSet) {
    std::string usedLocation;
    if (const CallICFGNode *callNode = SVFUtil::dyn_cast<CallICFGNode>(node)) {
        usedLocation = callNode->getCallSite()->getSourceLoc();
    } else if (const IntraICFGNode *intraNode = SVFUtil::dyn_cast<IntraICFGNode>(node)) {
        usedLocation = intraNode->getInst()->getSourceLoc();
    } else {
        assert(false && "not a call node or intra node?");
    }
    Log(LogLevel::Error) << SVFUtil::bugMsg1("\t USE_AFTER_FREE :") << " memory allocation at : ("
                         << _curEvalSVFGNode->getValue()->getSourceLoc()
                         << " used location at: ("
                         << usedLocation << ")\n";
    Dump() << SVFUtil::bugMsg1("\t USE_AFTER_FREE :") << " memory allocation at : ("
           << _curEvalSVFGNode->getValue()->getSourceLoc()
           << " used location at: ("
           << usedLocation << ")\n";
}


void UAFDetectorBase::reportBug() {
    // get src
    ICFGNode *src = _icfg->getICFGNode(_curEvalICFGNode->getId());
    // src is deleted because no PI is found
    if (!src || src->getInEdges().empty()) return;
    bool srcReachable = false;
    // check whether the reachability holds
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
    Set<const ICFGNode *> errNodes;
    Set<const ICFGNode *> reachableNodes;
    Map<const ICFGNode *, Z3Expr> errNodesToBranchCond;
    Map<const ICFGNode *, KeyNodesSet> errNodesToKeyNodes;
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
        // only report the first usage point resulting in $error.
        bool hasError = false;
        for (const auto &inEdge: getInTEdges(evalNode)) {
            if (hasError) break;
            for (const auto &absState: getFSMParser()->getAbsStates()) {
                if (hasError) break;
                if (absState == getFSMParser()->getErrAbsState()) continue;
                InfoKey tmpInfoKey = std::make_pair(
                        inEdge,
                        absState);
                auto it = _infoMap.find(tmpInfoKey);
                if (it != _infoMap.end()) {
                    auto symStates = it->second;
                    for (const auto &s: symStates) {
                        if (s.first == getFSMParser()->getErrAbsState()) {
                            hasError = true;
                            break;
                        }
                    }
                }
            }
        }
        if (hasError) continue;
        for (const auto &t: getOutTEdges(evalNode)) {
            for (const auto &absState: getFSMParser()->getAbsStates()) {
                if (absState == getFSMParser()->getErrAbsState()) continue;
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
                            errNodesToBranchCond[node] = s.second.getBranchCondition();
                            errNodesToKeyNodes[node] = SVFUtil::move(s.second.getKeyNodesSet());
                            hasError = true;
                            break;
                        }
                    }
                }
            }
        }
    }
    for (const auto &node: errNodes) {
        reportUAF(node, errNodesToBranchCond[node], errNodesToKeyNodes[node]);
    }
    if (PSAOptions::ValidateTests()) {
        for (const auto &node: _snks) {
            if (reachableNodes.count(node)) {
                if (errNodes.count(node))
                    validateSuccessTests(UAF_ERROR, node);
                else
                    validateSuccessTests(SAFE, node);
            }
        }
        for (const auto &node: errNodes) {
            if (reachableNodes.count(node)) {
                if (errNodes.count(node))
                    validateExpectedFailureTests(UAF_ERROR, node);
                else
                    validateExpectedFailureTests(SAFE, node);
            }
        }
    }
}

void UAFDetectorBase::validateSuccessTests(UAF_TYPE uafType, const ICFGNode *node) {
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

    if (name == "SAFEUAFFUNC") {
        if (uafType == SAFE)
            success = true;
    } else if (name == "UAFFUNC") {
        if (uafType == UAF_ERROR)
            success = true;
    } else {
        writeWrnMsg("\t can not validate, check function not found, please put it at the right place!!");
        return;
    }
    std::string usedLocation;
    if (const CallICFGNode *callNode = SVFUtil::dyn_cast<CallICFGNode>(node)) {
        usedLocation = callNode->getCallSite()->getSourceLoc();
    } else if (const IntraICFGNode *intraNode = SVFUtil::dyn_cast<IntraICFGNode>(node)) {
        usedLocation = intraNode->getInst()->getSourceLoc();
    } else {
        assert(false && "not a call node or intra node?");
    }
    if (success)
        outs() << SVFUtil::sucMsg("\t SUCCESS :") << name << " check <src id:" << _curEvalICFGNode->getId() << "> at ("
               << callBlockNode->getCallSite()->getSourceLoc() << " used location at: ("
               << usedLocation << ")\n";
    else {
        SVFUtil::errs() << SVFUtil::errMsg("\t FAILURE :") << name << " check <src id:" << _curEvalICFGNode->getId()
                        << callBlockNode->getCallSite()->getSourceLoc() << " used location at: ("
                        << usedLocation << ")\n";
        assert(false && "test case failed!");
    }
}

void UAFDetectorBase::validateExpectedFailureTests(UAF_TYPE uafType, const ICFGNode *node) {
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

    if (name == "UAFFUNC_FP") {
        if (uafType == UAF_ERROR)
            expectedFailure = true;
    } else if (name == "SAFEUAFFUNC") {
        return;
    } else {
        writeWrnMsg("\t can not validate, check function not found, please put it at the right place!!");
        return;
    }
    std::string usedLocation;
    if (const CallICFGNode *callNode = SVFUtil::dyn_cast<CallICFGNode>(node)) {
        usedLocation = callNode->getCallSite()->getSourceLoc();
    } else if (const IntraICFGNode *intraNode = SVFUtil::dyn_cast<IntraICFGNode>(node)) {
        usedLocation = intraNode->getInst()->getSourceLoc();
    } else {
        assert(false && "not a call node or intra node?");
    }

    if (expectedFailure)
        outs() << SVFUtil::sucMsg("\t EXPECTED-FAILURE :") << name << " check <src id:" << _curEvalICFGNode->getId()
               << callBlockNode->getCallSite()->getSourceLoc() << " used location at: ("
               << usedLocation << ")\n";
    else {
        SVFUtil::errs() << SVFUtil::errMsg("\t UNEXPECTED FAILURE :") << name
                        << " check <src id:" << _curEvalICFGNode->getId()
                        << callBlockNode->getCallSite()->getSourceLoc() << " used location at: ("
                        << usedLocation << ")\n";
        assert(false && "test case failed!");
    }
}
