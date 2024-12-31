//
// Created by Xiao on 2022/4/3.
//

#include "Slicing/PIExtractor.h"
#include "PSTA/PSAOptions.h"
#include "SABER/SaberSVFGBuilder.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "PSTA/Logger.h"

using namespace SVF;
using namespace SVFUtil;
using namespace std;


void PIExtractor::collectKeptNodes(Set<u32_t> &kept) {
    ICFG *icfg = PAG::getPAG()->getICFG();
    FIFOWorkList<u32_t> workList;
    Set<u32_t> visited;
    for (const auto &item: getAbsTransitionHandler()->getICFGAbsTransferMap()) {
        kept.insert(item.first->getId());
        if (const CallICFGNode *callNode = SVFUtil::dyn_cast<CallICFGNode>(item.first)) {
            kept.insert(callNode->getRetICFGNode()->getId());
        }
        PTACallGraphNode *callgraphNode = getPTACallGraph()->getCallGraphNode(item.first->getFun());
        if (!visited.count(callgraphNode->getId())) {
            workList.push(callgraphNode->getId());
            visited.insert(callgraphNode->getId());
            kept.insert(getICFGWrapper()->getFunEntry(item.first->getFun())->getId());
            kept.insert(getICFGWrapper()->getFunExit(item.first->getFun())->getId());
        }
    }
    while (!workList.empty()) {
        u32_t curItem = workList.pop();
        for (const auto &e: getPTACallGraph()->getCallGraphNode(curItem)->getInEdges()) {
            const CallICFGNode *callSite = getPTACallGraph()->getCallSite(e->getCallSiteID());
            NodeID nodeId = callSite->getId();
            kept.insert(nodeId);
            kept.insert(callSite->getRetICFGNode()->getId());
            if (visited.count(e->getSrcID())) continue;
            visited.insert(e->getSrcID());
            kept.insert(getICFGWrapper()->getFunEntry(callSite->getFun())->getId());
            kept.insert(getICFGWrapper()->getFunExit(callSite->getFun())->getId());
            workList.push(e->getSrcID());
        }
    }
}


void PIExtractor::compactICFGWrapper() {
    Set<u32_t> kept;
    collectKeptNodes(kept);
    Set<ICFGNodeWrapper *> callNodesToCompact, intraNodesToCompact;
    for (const auto &item: *getICFGWrapper()) {
        if (kept.count(item.second->getId())) continue;
        if (SVFUtil::isa<CallICFGNode>(item.second->getICFGNode())) {
            callNodesToCompact.insert(item.second);
        } else if (SVFUtil::isa<IntraICFGNode>(item.second->getICFGNode())) {
            intraNodesToCompact.insert(item.second);
        }
    }
    for (const auto &callNode: callNodesToCompact) {
        compactCallNodes(callNode);
    }

    for (const auto &intraNode: intraNodesToCompact) {
        compactIntraNodes(intraNode);
    }

}

void PIExtractor::initialize(SVFModule *svfModule) {
    PAG *pag = PAG::getPAG();
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
    compactICFGWrapper();
    _mainEntry = getICFGWrapper()->getFunEntry(getAbsTransitionHandler()->getMainFunc());
    assert(_mainEntry && "no main function?");
}


/*!
 * Given a pattern, extract all the operation sequence for each src, using enumeration
 * @param srcs
 * @param srcToSQ
 * @param pattern
 */
void PIExtractor::extractSQbyEnumeration(SrcSet &srcs, SrcToPI &srcToSQ, DataFact &pattern) {
    Log(LogLevel::Info) << "extracting seqs...";
    Dump() << "extracting seqs...";
    for (const auto &src: srcs) {
        Map<u32_t, Set<u32_t>> actionToNodes;
        for (const auto &mpItem: getAbsTransitionHandler()->getICFGAbsTransferMap()) {
            if (const CallICFGNode *callBlockNode = dyn_cast<CallICFGNode>(mpItem.first)) {
                if (callBlockNode == src->getICFGNode() || callBlockNode->getRetICFGNode() == src->getICFGNode()) {
                    // for src action
                    actionToNodes[getFSMParser()->getSrcAction()].insert(callBlockNode->getRetICFGNode()->getId());
                } else {
                    Set<const SVFFunction *> functionSet;
                    getPTACallGraph()->getCallees(callBlockNode, functionSet);
                    if (functionSet.empty())
                        continue;
                    FSMParser::FSMAction action = getFSMParser()->getTypeFromStr(
                            (*functionSet.begin())->getName());
                    // for non-src actions
                    if (getFSMParser()->getFSMActions().find(action) != getFSMParser()->getFSMActions().end() &&
                        action != getFSMParser()->getSrcAction()) {
                        actionToNodes[action].insert(callBlockNode->getRetICFGNode()->getId());
                    }
                }
            } else if (const IntraICFGNode *intraICFGNode = dyn_cast<IntraICFGNode>(mpItem.first)) {
                if (pattern.back() == FSMParser::CK_USE) {
                    if ((PSAOptions::MaxSnkLimit() == 0 ||
                         actionToNodes[FSMParser::CK_USE].size() < PSAOptions::MaxSnkLimit())) {
                        std::list<const SVFStmt *> svfStmts = intraICFGNode->getSVFStmts();
                        if (!svfStmts.empty()) {
                            for (const auto &svfStmt: svfStmts) {
                                if (isa<LoadStmt>(svfStmt)) {
                                    actionToNodes[FSMParser::CK_USE].insert(mpItem.first->getId());
                                }
                            }
                        }
                    }
                }
            } else {
                DBOUT(DGENERAL, outs() << pasMsg("no snkMap\n"));
            }
        }

        bool noSQ = false;
        for (const auto &action: pattern) {
            auto acit = actionToNodes.find(action);
            // If no action found, there is no violating sq
            if (acit == actionToNodes.end()) {
                noSQ = true;
                break;
            }
        }
        if (noSQ) continue;
        PI seqs;
        DataFact tmp;
        enumerateSQ(0, pattern, seqs, tmp, actionToNodes);
        srcToSQ[src] = seqs;
    }
    Log(LogLevel::Info) << SVFUtil::sucMsg("[done]\n");
    Dump() << SVFUtil::sucMsg("[done]\n");
}

/*!
 * Enumerate PI given separate nodes
 * @param index
 * @param pattern
 * @param seqs
 * @param tmp
 * @param actionToNodes
 */
void PIExtractor::enumerateSQ(u32_t index, DataFact &pattern, PI &seqs,
                              DataFact &tmp, Map<u32_t, Set<u32_t>> actionToNodes) {
    if (index == pattern.size()) {
        DataFact revSeq(tmp.rbegin(), tmp.rend());
        seqs.insert(revSeq);
        return;
    }
    for (const auto &n: actionToNodes[pattern[index]]) {
        tmp.push_back(n);
        enumerateSQ(index + 1, pattern, seqs, tmp, actionToNodes);
        tmp.pop_back();
    }
}

void PIExtractor::initMap(SVFModule *svfModule) {
    clearMap();
    _summaryMap.clear();
}


void PIExtractor::clearMap() {
    for (const auto &item: *getICFGWrapper()) {
        for (const auto &edge: item.second->getOutEdges()) {
            edge->_piInfoMap.clear();
        }
    }
}

/*!
 * Given snk types, extract all the operation sequences for each src
 * @param srcs
 * @param srcToSQ
 * @param checkerTypes
 */
void
PIExtractor::extract(SVFModule *svfModule, const SVFGNode *src, PI &sQ, Set<FSMParser::CHECKER_TYPE> &checkerTypes,
                     const ICFGNodeSet &snks) {
    _snks = snks;
    Log(LogLevel::Info) << "Extracting PI...\n";
    Dump() << "Extracting PI...\n";
    if (_snks.empty()) {
        Log(LogLevel::Info) << "PI size: 0\n";
        Dump() << "PI size: 0\n";
        Log(LogLevel::Info) << SVFUtil::sucMsg("Extracting PI...[done]\n");
        Dump() << SVFUtil::sucMsg("Extracting PI...[done]\n");
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
//    PSAUtil::logMsg("Cur eval ICFG node: " + _curEvalICFGNode->toString() + "\n", 0);
    _PIStateManager.setCurEvalICFGNode(_curEvalICFGNode);
    _PIStateManager.setCheckerTypes(checkerTypes);
    _PIStateManager.setCurSnks(_snks);
    initMap(svfModule);
    solve();
    PI seqsTmp, seqs;
    if (const CallICFGNode *n = dyn_cast<CallICFGNode>(src->getICFGNode()))
        collectPI(seqsTmp, _snks, n->getRetICFGNode());
    else
        collectPI(seqsTmp, _snks, src->getICFGNode());
    for (const auto &df: seqsTmp) {
        DataFact revDf(df.rbegin(), df.rend());
        seqs.insert(revDf);
    }
    Log(LogLevel::Info) << "PI size: " << std::to_string(seqs.size()) << "\n";
    Dump() << "PI size: " << std::to_string(seqs.size()) << "\n";
    Log(LogLevel::Info) << SVFUtil::sucMsg("Extracting PI...[done]\n");
    Dump() << SVFUtil::sucMsg("Extracting PI...[done]\n");
//        vector<DataFact> debug(seqs.begin(), seqs.end());
//    PSAUtil::logMsg("seqs size: " + std::to_string(seqs.size()) + "\n", 0);
    sQ = seqs;
    clearMap();
}

void PIExtractor::collectPI(PI &seqs, OrderedSet<const ICFGNode *> &sks, const ICFGNode *curEvalICFGNode) {
    for (const auto &node: sks) {
        const ICFGNodeWrapper *evalNode = getICFGWrapper()->getICFGNodeWrapper(node->getId());
        if (const CallICFGNode *callNode = dyn_cast<CallICFGNode>(node)) {
            evalNode = evalNode->getRetICFGNodeWrapper();
        }
        for (const auto &t: getOutTEdges(evalNode)) {
            for (const auto &absState: getFSMParser()->getAbsStates()) {
                auto it = t->_piInfoMap.find(absState);
                if (it != t->_piInfoMap.end()) {
                    for (const auto &s: it->second) {
                        if (PSAOptions::LEAK()) {
                            // to detect partial free
                            if (s.first == TypeState::Freed) {
                                seqs.insert(s.second.getPI().begin(), s.second.getPI().end());
                            }
                        } else {
                            if (s.first == getFSMParser()->getErrAbsState()) {
//                            seqs.insert(s.second->getPI().begin(), s.second->getPI().end());
                                Set<const ICFGNodeWrapper *> visited;
                                collectUpper(evalNode, evalNode, seqs, curEvalICFGNode, visited);
                            }
                        }
                    }
                }
            }
        }
    }
}

void PIExtractor::collectUpper(const ICFGNodeWrapper *curNode, const ICFGNodeWrapper *evalNode, PI &seqs,
                               const ICFGNode *curEvalICFGNode, Set<const ICFGNodeWrapper *> &visited) {
    if (visited.count(curNode)) return;
    visited.insert(curNode);
    for (const auto &t: getOutTEdges(curNode)) {
        for (const auto &absState: getFSMParser()->getAbsStates()) {
            auto it = t->_piInfoMap.find(absState);
            if (it != t->_piInfoMap.end()) {
                for (const auto &s: it->second) {
                    if (s.first == getFSMParser()->getErrAbsState()) {
                        for (const auto &pi: s.second.getPI()) {
                            if (curEvalICFGNode->getId() == pi.front()) {
                                if (pi.back() == evalNode->getId()) {
                                    seqs.insert(pi);
                                } else {

                                }
                            } else {
                                if (const SVFFunction *fun = curNode->getICFGNode()->getFun()) {
                                    const ICFGNodeWrapper *exitNode = getICFGWrapper()->getFunExit(fun);
                                    for (const auto &outEdge: exitNode->getOutEdges()) {
                                        if (outEdge->getICFGEdge()->isRetCFGEdge()) {
                                            collectUpper(outEdge->getDstNode(), evalNode, seqs, curEvalICFGNode,
                                                         visited);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}


void PIExtractor::solve() {
    WorkList workList;

    PIState s(getFSMParser()->getUninitAbsState(), {});
    for (const auto &e: _mainEntry->getOutEdges()) {
        addInfo(e, getFSMParser()->getUninitAbsState(), s);
        WLItem firstItem(e->getDstNode(), getFSMParser()->getUninitAbsState(), getFSMParser()->getUninitAbsState());
        workList.push(firstItem);
    }

    while (!workList.empty()) {
        WLItem curItem = workList.pop();
        if (const CallICFGNode *callBlockNode = isCallNode(curItem.getICFGNodeWrapper())) {
            PIState sqState;
            if (isMergeNode(curItem.getICFGNodeWrapper()))
                sqState = std::move(mergeFlowFun(curItem.getICFGNodeWrapper(), curItem.getTypeState(),
                                                 curItem.getIndexTypeState()));
            else
                sqState = getInfo(*curItem.getICFGNodeWrapper()->getInEdges().begin(), curItem.getTypeState(),
                                  curItem.getIndexTypeState());
            // add summary and add trigger for callee (Lines 15-18)
            FunctionSet funSet;
            callee(callBlockNode, funSet);
            for (const auto &fun: funSet) {
                bool addedTrigger = false;
                for (const auto &absState: getFSMParser()->getAbsStates()) {
                    PIState &summary = getSummary(fun, sqState.getAbstractState(), absState);
                    // Apply summary if summary is not empty
                    if (!summary.isNullPI()) {
                        addedTrigger = true;
                        PIState newSQState = std::move(applySummary(sqState, summary));
                        TypeState nxtTypeState = newSQState.getAbstractState();
                        const Set<const ICFGEdgeWrapper *> &outEdges = getOutTEdges(
                                nextNodeToAdd(curItem.getICFGNodeWrapper()));
                        if (outEdges.size() == 1) {
                            if (addInfo(*outEdges.begin(), curItem.getTypeState(), std::move(newSQState))) {
                                workList.push(
                                        WLItem((*outEdges.begin())->getDstNode(), curItem.getTypeState(),
                                               nxtTypeState));
                            }
                        } else {
                            for (const auto &outEdge: getOutTEdges(nextNodeToAdd(curItem.getICFGNodeWrapper()))) {
                                if (addInfo(outEdge, curItem.getTypeState(), newSQState)) {
                                    workList.push(
                                            WLItem(outEdge->getDstNode(), curItem.getTypeState(),
                                                   nxtTypeState));
                                }
                            }
                        }
                    }
                }
                if (!addedTrigger) {
                    PIState piStateTrigger(sqState.getAbstractState(), {});
                    const ICFGNodeWrapper *entry = entryNode(fun);
                    for (const auto &entryE: entry->getOutEdges()) {
                        if (addTrigger(entryE, piStateTrigger)) {
                            workList.push(WLItem(entryE->getDstNode(), piStateTrigger.getAbstractState(), piStateTrigger.getAbstractState()));
                        }
                    }
                }
            }
        } else if (isExitNode(curItem.getICFGNodeWrapper())) {
            PIState sqState;
            if (isMergeNode(curItem.getICFGNodeWrapper()))
                sqState = std::move(mergeFlowFun(curItem.getICFGNodeWrapper(), curItem.getTypeState(),
                                                 curItem.getIndexTypeState()));
            else
                sqState = getInfo(*curItem.getICFGNodeWrapper()->getInEdges().begin(), curItem.getTypeState(),
                                  curItem.getIndexTypeState());
            nonBranchFlowFun(curItem.getICFGNodeWrapper(), &sqState);
            if (addToSummary(curItem.getICFGNodeWrapper(), curItem.getTypeState(), sqState)) {
                std::vector<const ICFGNodeWrapper *> retNodes;
                returnSites(curItem.getICFGNodeWrapper(), retNodes);
                for (const auto &retSite: retNodes) {
                    for (const auto &formalInAs: getFSMParser()->getAbsStates()) {
                        for (const auto &inEdge: getInTEdges(callSite(retSite))) {
                            for (const auto &callerAs: getFSMParser()->getAbsStates()) {
                                if (!getInfo(const_cast<ICFGEdgeWrapper *>(inEdge), callerAs, formalInAs).isNullPI()) {
                                    if (hasSummary(fn(curItem.getICFGNodeWrapper()), formalInAs)) {
                                        workList.push(WLItem(callSite(retSite), callerAs, formalInAs));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else if (isBranchNode(curItem.getICFGNodeWrapper())) {
            for (const auto &edge: curItem.getICFGNodeWrapper()->getOutEdges()) {
                if (const IntraCFGEdge *intraCfgEdge = SVFUtil::dyn_cast<IntraCFGEdge>(edge->getICFGEdge())) {
                    if (intraCfgEdge->getCondition()) {
                        PC_TYPE brCond = evalBranchCond(intraCfgEdge);
                        if (brCond != PC_TYPE::FALSE_PC) {
                            PIState sqState;
                            if (isMergeNode(curItem.getICFGNodeWrapper()))
                                sqState = std::move(mergeFlowFun(curItem.getICFGNodeWrapper(), curItem.getTypeState(),
                                                                 curItem.getIndexTypeState()));
                            else
                                sqState = getInfo(*curItem.getICFGNodeWrapper()->getInEdges().begin(),
                                                  curItem.getTypeState(),
                                                  curItem.getIndexTypeState());
                            if (addInfo(edge, curItem.getTypeState(), sqState)) {
                                workList.push(
                                        WLItem(edge->getDstNode(), curItem.getTypeState(),
                                               curItem.getIndexTypeState()));
                            }
                        }
                    }
                }
            }
        } else {
            PIState piState;
            if (isMergeNode(curItem.getICFGNodeWrapper()))
                piState = std::move(mergeFlowFun(curItem.getICFGNodeWrapper(), curItem.getTypeState(),
                                                 curItem.getIndexTypeState()));
            else
                piState = getInfo(*curItem.getICFGNodeWrapper()->getInEdges().begin(),
                                  curItem.getTypeState(),
                                  curItem.getIndexTypeState());
            nonBranchFlowFun(curItem.getICFGNodeWrapper(), &piState);
            TypeState nxtTypeState = piState.getAbstractState();
            const Set<const ICFGEdgeWrapper *> &outEdges = getOutTEdges(
                    nextNodeToAdd(curItem.getICFGNodeWrapper()));
            if (outEdges.size() == 1) {
                if (addInfo(*outEdges.begin(), curItem.getTypeState(), std::move(piState))) {
                    workList.push(
                            WLItem((*outEdges.begin())->getDstNode(), curItem.getTypeState(),
                                   nxtTypeState));
                }
            } else {
                for (const auto &outEdge: getOutTEdges(nextNodeToAdd(curItem.getICFGNodeWrapper()))) {
                    if (addInfo(outEdge, curItem.getTypeState(), piState)) {
                        workList.push(
                                WLItem(outEdge->getDstNode(), curItem.getTypeState(),
                                       nxtTypeState));
                    }
                }
            }
        }
    }
}


bool PIExtractor::isTestContainsNullAndTheValue(const CmpStmt *cmp) {
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
PIExtractor::PC_TYPE PIExtractor::evaluateTestNullLikeExpr(const BranchStmt *cmpInst, const IntraCFGEdge *edge) {
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


PIExtractor::PC_TYPE PIExtractor::evalBranchCond(const IntraCFGEdge *intraCfgEdge) {
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
void PIExtractor::collectBBCallingProgExit(const SVFBasicBlock &bb) {

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
bool PIExtractor::isBBCallsProgExit(const SVFBasicBlock *bb) {
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
PIExtractor::PC_TYPE PIExtractor::evaluateProgExit(const BranchStmt *brInst, const SVFBasicBlock *succ) {
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
