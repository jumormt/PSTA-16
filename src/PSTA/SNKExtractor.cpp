//
// Created by z5489727 on 2023/7/25.
//

#include "PSTA/SNKExtractor.h"
#include "PSTA/Logger.h"

using namespace SVF;
using namespace SVFUtil;


void SNKExtractor::initialize(SVFModule *svfModule) {
    for (const auto &func: *svfModule) {
        if (!SVFUtil::isExtCall(func)) {
            // Allocate conditions for a program.
            for (SVFFunction::const_iterator bit = func->begin(), ebit = func->end();
                 bit != ebit; ++bit) {
                const SVFBasicBlock *bb = *bit;
                collectBBCallingProgExit(*bb);
            }
        }
    }
    _mainEntry = getICFGWrapper()->getFunEntry(getAbsTransitionHandler()->getMainFunc());
    assert(_mainEntry && "no main function?");
}


void SNKExtractor::initMap() {
    clearMap();
    _summaryMap.clear();
}


void SNKExtractor::clearMap() {
    for (const auto &item: *getICFGWrapper()) {
        for (const auto &edge: item.second->getOutEdges()) {
            edge->_snkInfoMap.clear();
        }
    }
}

void
SNKExtractor::extract(SVF::SVFModule *svfModule, const SVF::SVFGNode *src, SNKExtractor::ICFGNodeSet &snks) {
    _snks = snks;
    Log(LogLevel::Info) << "Extracting Snks...\n";
    Dump() << "Extracting Snks...\n";
    if (_snks.empty()) {
        Log(LogLevel::Info) << "Snks size: 0\n";
        Dump() << "Snks size: 0\n";
        Log(LogLevel::Info) << SVFUtil::sucMsg("Extracting Snks...[done]\n");
        Dump() << SVFUtil::sucMsg("Extracting Snks...[done]\n");
        return;
    }
    initialize(svfModule);

    _curEvalSVFGNode = src;
    if (const RetICFGNode *n = dyn_cast<RetICFGNode>(src->getICFGNode()))
        _curEvalICFGNode = n->getCallICFGNode();
    else
        _curEvalICFGNode = src->getICFGNode();
    _curEvalFuns.clear();
    for (const auto &e: _curEvalICFGNode->getOutEdges()) {
        if (const CallCFGEdge *callEdge = dyn_cast<CallCFGEdge>(e)) {
            _curEvalFuns.insert(callEdge->getDstNode()->getFun());
        }
    }
    initMap();
    solve();
    
    collectSnks();
    snks = SVFUtil::move(_snks);

    Log(LogLevel::Info) << "Snks size: " << std::to_string(snks.size()) << "\n";
    Dump() << "Snks size: " << std::to_string(snks.size()) << "\n";
    Log(LogLevel::Info) << SVFUtil::sucMsg("Extracting Snks...[done]\n");
    Dump() << SVFUtil::sucMsg("Extracting Snks...[done]\n");
    clearMap();
}

void SNKExtractor::collectSnks() {

    const std::unique_ptr<ICFGWrapper> &icfgWrapper = ICFGWrapper::getICFGWrapper();
    // get src
    ICFGNodeWrapper *srcWrapper = icfgWrapper->getICFGNodeWrapper(_curEvalICFGNode->getId());
    bool srcReachable = false;
    // check whether the reachability holds
    for (const auto &e: srcWrapper->getInEdges()) {
        auto srcReachableIt = e->_snkInfoMap.find(getFSMParser()->getUninitAbsState());
        if (srcReachableIt != e->_snkInfoMap.end()) {
            srcReachable = true;
        }
    }
    // src unreachable
    if (!srcReachable) {
        _snks.clear();
        return;
    }
    Set<const ICFGNode *> errNodes;
    Set<const ICFGNode *> reachableNodes;
    ICFGNodeSet snkToRm;
    for (const auto &node: _snks) {
        const ICFGNodeWrapper *evalNode = icfgWrapper->getICFGNodeWrapper(node->getId());
        if (PSAOptions::Wrapper())
            evalNode = icfgWrapper->getICFGNodeWrapper(
                    dyn_cast<CallICFGNode>(node)->getRetICFGNode()->getId());
        else if (SVFUtil::isa<CallICFGNode>(evalNode->getICFGNode())) {
            evalNode = icfgWrapper->getICFGNodeWrapper(
                    dyn_cast<CallICFGNode>(node)->getRetICFGNode()->getId());
        }
        // snk not reachable
        if (!evalNode) {
            snkToRm.insert(node);
            continue;
        }
        // only report the first usage point resulting in $error.
        bool hasError = false;
        for (const auto &inEdge: getInTEdges(evalNode)) {
            if (hasError) break;
            for (const auto &absState: getFSMParser()->getAbsStates()) {
                if (hasError) break;
                if (absState == getFSMParser()->getErrAbsState()) continue;
                auto it = inEdge->_snkInfoMap.find(absState);
                if (it != inEdge->_snkInfoMap.end()) {
                    auto typeStates = it->second;
                    for (const auto &ts: typeStates) {
                        if (ts == getFSMParser()->getErrAbsState()) {
                            hasError = true;
                            break;
                        }
                    }
                }
            }
        }
        if (hasError) {
            snkToRm.insert(node);
            continue;
        }
        for (const auto &t: getOutTEdges(evalNode)) {
            for (const auto &absState: getFSMParser()->getAbsStates()) {
                if (absState == getFSMParser()->getErrAbsState()) continue;
                auto it = t->_snkInfoMap.find(absState);
                if (it != t->_snkInfoMap.end()) {
                    auto typeStates = it->second;
                    for (const auto &ts: typeStates) {
                        if (ts == getFSMParser()->getErrAbsState()) {
                            hasError = true;
                            break;
                        }
                    }
                }
            }
        }
        if(!hasError) {
            snkToRm.insert(node);
        }
    }
    for (const auto &node: snkToRm) {
        _snks.erase(node);
    }
}


void SNKExtractor::solve() {
    WorkList workList;

    ICFGEdgeWrapper *e = *_mainEntry->getOutEdges().begin();
    addInfo(e, getFSMParser()->getUninitAbsState(), getFSMParser()->getUninitAbsState());
    WLItem firstItem(e->getDstNode(), getFSMParser()->getUninitAbsState(), getFSMParser()->getUninitAbsState());

    workList.push(firstItem);

    while (!workList.empty()) {
        WLItem curItem = workList.pop();
        if (const CallICFGNode *callBlockNode = isCallNode(curItem.getICFGNodeWrapper())) {
            TypeState typeState = TypeState::Unknown;
            if (isMergeNode(curItem.getICFGNodeWrapper()))
                typeState = mergeFlowFun(curItem.getICFGNodeWrapper(), curItem.getTypeState(),
                                         curItem.getIndexTypeState());
            else
                typeState = getInfo(*curItem.getICFGNodeWrapper()->getInEdges().begin(), curItem.getTypeState(),
                                    curItem.getIndexTypeState());
            // add summary and add trigger for callee (Lines 15-18)
            FunctionSet funSet;
            callee(callBlockNode, funSet);
            for (const auto &fun: funSet) {
                bool addedTrigger = false;
                for (const auto &absState: getFSMParser()->getAbsStates()) {
                    TypeState summary = getSummary(fun, typeState, absState);
                    // Apply summary if summary is not empty
                    if (summary != TypeState::Unknown) {
                        addedTrigger = true;
                        const TypeState newTypeState = applySummary(typeState, summary);
                        for (const auto &outEdge: getOutTEdges(nextNodeToAdd(curItem.getICFGNodeWrapper()))) {
                            if (addInfo(outEdge, curItem.getTypeState(), newTypeState)) {
                                workList.push(
                                        WLItem(outEdge->getDstNode(), curItem.getTypeState(),
                                               newTypeState));
                            }
                        }
                    }
                }
                if (!addedTrigger) {
                    const ICFGNodeWrapper *entry = entryNode(fun);
                    if (addTrigger(entry, typeState)) {
                        workList.push(WLItem((*entry->getOutEdges().begin())->getDstNode(),
                                             typeState, typeState));
                    }
                }
            }
        } else if (isExitNode(curItem.getICFGNodeWrapper())) {
            const TypeState sqState = getInfo(*curItem.getICFGNodeWrapper()->getInEdges().begin(),
                                              curItem.getTypeState(), curItem.getIndexTypeState());
            const TypeState newSQState = nonBranchFlowFun(curItem.getICFGNodeWrapper(), sqState);
            if (addToSummary(curItem.getICFGNodeWrapper(), curItem.getTypeState(), sqState)) {
                std::vector<const ICFGNodeWrapper *> retNodes;
                returnSites(curItem.getICFGNodeWrapper(), retNodes);
                for (const auto &retSite: retNodes) {
                    for (const auto &formalInAs: getFSMParser()->getAbsStates()) {
                        for (const auto &inEdge: getInTEdges(callSite(retSite))) {
                            for (const auto &callerAs: getFSMParser()->getAbsStates()) {
                                if (getInfo(inEdge, callerAs, formalInAs) != TypeState::Unknown) {
                                    if (hasSummary(fn(curItem.getICFGNodeWrapper()), formalInAs)) {
                                        workList.push(WLItem(callSite(retSite), callerAs, formalInAs));
                                    }
                                }
                            }

                        }
                    }
                }
            }
        } else if (isMergeNode(curItem.getICFGNodeWrapper())) {
            const TypeState sqState = mergeFlowFun(curItem.getICFGNodeWrapper(), curItem.getTypeState(),
                                                   curItem.getIndexTypeState());
            for (const auto &outEdge: getOutTEdges(nextNodeToAdd(curItem.getICFGNodeWrapper()))) {
                if (addInfo(outEdge, curItem.getTypeState(), sqState)) {
                    workList.push(WLItem(outEdge->getDstNode(), curItem.getTypeState(), sqState));
                }
            }
        } else if (isBranchNode(curItem.getICFGNodeWrapper())) {
            for (const auto &edge: curItem.getICFGNodeWrapper()->getOutEdges()) {
                if (const IntraCFGEdge *intraCfgEdge = SVFUtil::dyn_cast<IntraCFGEdge>(edge->getICFGEdge())) {
                    if (intraCfgEdge->getCondition()) {
                        PC_TYPE brCond = evalBranchCond(intraCfgEdge);
                        if (brCond != PC_TYPE::FALSE_PC) {
                            if (addInfo(edge, curItem.getTypeState(),
                                        getInfo(*curItem.getICFGNodeWrapper()->getInEdges().begin(),
                                                curItem.getTypeState(), curItem.getIndexTypeState()))) {
                                workList.push(
                                        WLItem(edge->getDstNode(), curItem.getTypeState(),
                                               curItem.getIndexTypeState()));
                            }
                        }
                    }
                }
            }
        } else {
            const TypeState sqStateOut = nonBranchFlowFun(curItem.getICFGNodeWrapper(),
                                                          getInfo(*curItem.getICFGNodeWrapper()->getInEdges().begin(),
                                                                  curItem.getTypeState(), curItem.getIndexTypeState()));
            for (const auto &outEdge: getOutTEdges(nextNodeToAdd(curItem.getICFGNodeWrapper()))) {
                if (addInfo(outEdge, curItem.getTypeState(), sqStateOut)) {
                    workList.push(
                            WLItem(outEdge->getDstNode(), curItem.getTypeState(), sqStateOut));
                }
            }
        }
    }
}


bool SNKExtractor::isTestContainsNullAndTheValue(const CmpStmt *cmp) {
    SVFIR *pag = SVFIR::getPAG();
    AndersenWaveDiff *ander = AndersenWaveDiff::createAndersenWaveDiff(pag);
    const SVFValue *op0 = cmp->getOpVar(0)->getValue();
    const SVFValue *op1 = cmp->getOpVar(1)->getValue();
    if (SVFUtil::isa<SVFConstantNullPtr>(op1)) {
        const CallICFGNode *node = SVFUtil::dyn_cast<CallICFGNode>(_curEvalICFGNode);
        if (node->getSVFStmts().empty() && node->getRetICFGNode()->getSVFStmts().empty())
            return false;
        if (node->getSVFStmts().empty() || SVFUtil::isa<CallPE>(*node->getSVFStmts().begin())) {
            return ander->alias(pag->getValueNode(op0),
                                (*node->getRetICFGNode()->getSVFStmts().begin())->getDstID()) != NoAlias;
        } else {
            return ander->alias(pag->getValueNode(op0),
                                (*node->getSVFStmts().begin())->getDstID()) != NoAlias;
        }
    } else if (SVFUtil::isa<SVFConstantNullPtr>(op0)) {
        const CallICFGNode *node = SVFUtil::dyn_cast<CallICFGNode>(_curEvalICFGNode);
        if (node->getSVFStmts().empty() && node->getRetICFGNode()->getSVFStmts().empty())
            return false;
        if (node->getSVFStmts().empty() || SVFUtil::isa<CallPE>(*node->getSVFStmts().begin()))
            return ander->alias(pag->getValueNode(op1),
                                (*node->getRetICFGNode()->getSVFStmts().begin())->getDstID()) != NoAlias;
        else
            return ander->alias(pag->getValueNode(op1),
                                (*node->getSVFStmts().begin())->getDstID()) != NoAlias;
    }
    return false;
}

/*!
 * Evaluate null like expression for source-sink related bug detection in SABER
 * @param cmpInst
 * @param edge
 * @return
 */
SNKExtractor::PC_TYPE SNKExtractor::evaluateTestNullLikeExpr(const BranchStmt *cmpInst, const IntraCFGEdge *edge) {
    const SVFBasicBlock *succ1 = cmpInst->getSuccessor(0)->getBB();

    if (isTestNullExpr(cmpInst->getCondition()->getValue())) {
        // succ is then branch
        if (edge->getSuccessorCondValue() == 0)
            return PC_TYPE::TRUE_PC;
            // succ is else branch
        else
            return PC_TYPE::FALSE_PC;
    }
    if (isTestNotNullExpr(cmpInst->getCondition()->getValue())) {
        // succ is then branch
        if (edge->getSuccessorCondValue() == 0)
            return PC_TYPE::FALSE_PC;
            // succ is else branch
        else
            return PC_TYPE::TRUE_PC;
    }

    return PC_TYPE::UNK_PC;
}


SNKExtractor::PC_TYPE SNKExtractor::evalBranchCond(const IntraCFGEdge *intraCfgEdge) {
    const SVFBasicBlock *bb = intraCfgEdge->getSrcNode()->getBB();
    const SVFBasicBlock *succ = intraCfgEdge->getDstNode()->getBB();
    if (bb->getNumSuccessors() == 1) {
        assert(bb->getSuccessors().front() == succ && "not the unique successor?");
        return PC_TYPE::TRUE_PC;
    }
    if (ICFGNode *icfgNode = PAG::getPAG()->getICFG()->getICFGNode(bb->getTerminator())) {
        for (const auto &svfStmt: icfgNode->getSVFStmts()) {
            if (const BranchStmt *branchStmt = SVFUtil::dyn_cast<BranchStmt>(svfStmt)) {
                if (branchStmt->getNumSuccessors() == 2) {
                    PC_TYPE evalProgExit = evaluateProgExit(branchStmt, succ);
                    if (evalProgExit != PC_TYPE::UNK_PC)
                        return evalProgExit;
                    PC_TYPE evalTestNullLike = evaluateTestNullLikeExpr(branchStmt, intraCfgEdge);
                    if (evalTestNullLike != PC_TYPE::UNK_PC)
                        return evalTestNullLike;
                    break;
                }
            }
        }
    }
    return PC_TYPE::UNK_PC;

}

/*!
 * Whether this basic block contains program exit function call
 */
void SNKExtractor::collectBBCallingProgExit(const SVFBasicBlock &bb) {

    for (SVFBasicBlock::const_iterator it = bb.begin(), eit = bb.end(); it != eit; it++) {
        const SVFInstruction *svfInst = *it;
        if (SVFUtil::isCallSite(svfInst))
            if (SVFUtil::isProgExitCall(svfInst)) {
                const SVFFunction *svfun = bb.getParent();
                _funToExitBBsMap[svfun].insert(&bb);
            }
    }
}

/*!
 * Whether this basic block contains program exit function call
 */
bool SNKExtractor::isBBCallsProgExit(const SVFBasicBlock *bb) {
    const SVFFunction *svfun = bb->getParent();
    FunToExitBBsMap::const_iterator it = _funToExitBBsMap.find(svfun);
    if (it != _funToExitBBsMap.end()) {
        for (const auto &bit: it->second) {
            if (postDominate(bit, bb))
                return true;
        }
    }
    return false;
}


/*!
 * Evaluate condition for program exit (e.g., exit(0))
 */
SNKExtractor::PC_TYPE SNKExtractor::evaluateProgExit(const BranchStmt *brInst, const SVFBasicBlock *succ) {
    const SVFBasicBlock *succ1 = brInst->getSuccessor(0)->getBB();
    const SVFBasicBlock *succ2 = brInst->getSuccessor(1)->getBB();

    bool branch1 = isBBCallsProgExit(succ1);
    bool branch2 = isBBCallsProgExit(succ2);

    /// then branch calls program exit
    if (branch1 && !branch2) {
        // succ is then branch
        if (succ1 == succ)
            return PC_TYPE::FALSE_PC;
            // succ is else branch
        else
            return PC_TYPE::TRUE_PC;
    }
        /// else branch calls program exit
    else if (!branch1 && branch2) {
        // succ is else branch
        if (succ2 == succ)
            return PC_TYPE::FALSE_PC;
            // succ is then branch
        else
            return PC_TYPE::TRUE_PC;
    }
        // two branches both call program exit
    else if (branch1 && branch2) {
        return PC_TYPE::FALSE_PC;
    }
        /// no branch call program exit
    else
        return PC_TYPE::UNK_PC;

}




