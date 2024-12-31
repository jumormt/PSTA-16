//
// Created by Xiao on 7/26/2022.
//

#include "PSTA/PSTA.h"

#include "SVF-LLVM/SVFIRBuilder.h"
#include "Slicing/ControlDG.h"
#include "PSTA/PSAStat.h"
#include "PSTA/BTPExtractor.h"
#include <queue>
#include <numeric>
#include "PSTA/Logger.h"



using namespace SVF;
using namespace SVFUtil;
using namespace std;




PSTA::PSTA() : _stat(new PSAStat(this)), _graphSparsificator() {
    _graphSparsificator.setStat(_stat);
}

PSTA::~PSTA() {
    _infoMap.clear();
    delete _stat;
    _stat = nullptr;
    ControlDG::releaseControlDG();
    BranchAllocator::releaseCondAllocator();
}

/*!
 * Initialization
 * @param module
 * @param _fsmFile
 */
void PSTA::initialize(SVFModule *module) {
    PSTABase::initialize(module);
    // Allocate branch condition
    Log(LogLevel::Info) << "Allocating branch condition...";
    Dump() << "Allocating branch condition...";
    // TODO: add a comment
    getPathAllocator()->allocate();
    Log(LogLevel::Info) << SVFUtil::sucMsg("[done]\n");
    Dump() << SVFUtil::sucMsg("[done]\n");
    _stat->wrapStart();
    // Clone ICFG to ICFGWrapper
    ICFGWrapperBuilder icfgWrapperBuilder;
    icfgWrapperBuilder.build(_icfg);
    _stat->wrapEnd();
}

void PSTA::initHandler(SVFModule *module) {
    ICFGWrapperBuilder builder;
    ICFG *icfg = PAG::getPAG()->getICFG();
    builder.build(icfg);
    _graphSparsificator.clearItems();
}

/*!
 * Initialize info and summary map
 * @param module svf module
 */
void PSTA::initMap(SVFModule *module) {
    _infoMap.clear();
    _summaryMap.clear();
}

/*!
 * Analyzing entry
 * @param module
 * @param _fsmFile
 */
void PSTA::analyze(SVFModule *module) {
    initialize(module);
    _stat->solveStart();
    Log(LogLevel::Info) << "PSTA solve...\n";
    Log(LogLevel::Info) << "****************************************************\n";
    Dump() << "PSTA solve...\n";
    Dump() << "****************************************************\n";
    int solveCt = 0;
    // iterate each src and run the solver
    for (const auto &item: _srcs) {
        // Set current evaluated SVFG and ICFG node
        _curEvalSVFGNode = item;
        _curEvalICFGNode = item->getICFGNode();
        _curEvalFuns.clear();
        // Set ICFGNode with corresponding CallICFGNode
        if (const RetICFGNode *n = dyn_cast<RetICFGNode>(_curEvalICFGNode))
            _curEvalICFGNode = n->getCallICFGNode();
        for (const auto &e: _curEvalICFGNode->getOutEdges()) {
            if (const CallCFGEdge *callEdge = dyn_cast<CallCFGEdge>(e)) {
                _curEvalFuns.insert(callEdge->getDstNode()->getFun());
            }
        }
        Log(LogLevel::Info) << std::to_string(++solveCt) << "/" << std::to_string(_srcs.size());
        Dump() << std::to_string(solveCt) << "/" << std::to_string(_srcs.size());
        Log(LogLevel::Info) << " Cur eval ICFG node: " << _curEvalICFGNode->toString() << "\n";
        Dump() << " Cur eval ICFG node: " << _curEvalICFGNode->toString() << "\n";

        _symStateMgr.setCurEvalICFGNode(_curEvalICFGNode);
        _symStateMgr.setCurEvalFuns(_curEvalFuns);
        if (PSAOptions::EvalNode() != 0 && _curEvalICFGNode->getId() != PSAOptions::EvalNode()) {
            Log(LogLevel::Info)
                    << SVFUtil::sucMsg("[skip] solving " + std::to_string(_curEvalICFGNode->getId()) + "\n");
            Dump() << SVFUtil::sucMsg("[skip] solving " + std::to_string(_curEvalICFGNode->getId()) + "\n");
            Log(LogLevel::Info) << "-------------------------------------------------------\n";
            Dump() << "-------------------------------------------------------\n";
            continue;
        }
        initHandler(module);
        if (PSAOptions::MultiSlicing() && _graphSparsificator.getSQ().empty()) {
            Log(LogLevel::Info)
                    << SVFUtil::sucMsg("[done] solving " + std::to_string(_curEvalICFGNode->getId()) + "\n");
            Dump() << SVFUtil::sucMsg("[done] solving " + std::to_string(_curEvalICFGNode->getId()) + "\n");
            Log(LogLevel::Info) << "-------------------------------------------------------\n";
            Dump() << "-------------------------------------------------------\n";
            continue;
        }
        // Compact graph with slice
        if (PSAOptions::PathSensitive()) {
            _stat->compactGraphStart();
            _graphSparsificator.compactGraph(_curEvalSVFGNode, _curEvalICFGNode, _curEvalFuns, _mainEntry, _snks);
            _stat->compactGraphEnd();
        }
        if (PSAOptions::DumpICFGWrapper())
            continue;
        initMap(module);
        solve();
        _stat->collectCompactedGraphStats();
        if (PSAOptions::PrintPathCond())
            printSS();
        reportBug();
        Log(LogLevel::Info) << "[done] solving " << std::to_string(_curEvalICFGNode->getId()) << "\n";
        Dump() << "[done] solving " << std::to_string(_curEvalICFGNode->getId()) << "\n";
        Log(LogLevel::Info) << "-------------------------------------------------------\n";
        Dump() << "-------------------------------------------------------\n";
    }
    _stat->solveEnd();
}

/*!
 * Main algorithm
 */
void PSTA::solve() {
    SymState initSymState(SVFUtil::move(ConsExeState::initExeState()),
                          getFSMParser()->getUninitAbsState());
    ICFGNodeWrapper *mainEntryNode = getICFGWrapper()->getICFGNodeWrapper(_mainEntry->getId());
    // multi-point slicing - no unsafe sequence
    if (mainEntryNode->getOutEdges().empty()) return;
    for (const auto &e: mainEntryNode->getOutEdges()) {
        addInfo(e, getFSMParser()->getUninitAbsState(), initSymState);
        WLItem firstItem(e->getDstNode(), getFSMParser()->getUninitAbsState(), getFSMParser()->getUninitAbsState());
        _workList.push(firstItem);
    }
    while (!_workList.empty()) {
        WLItem curItem = _workList.pop();
        // Process CallICFGNode
        //
        // Generate function summary, apply summary
        // if summary is generated already
        if (const CallICFGNode *callBlockNode = isCallNode(curItem.getICFGNodeWrapper())) {
            if (PSAOptions::PathSensitive() && PSAOptions::EnableIsoSummary())
                processCallNodeIso(callBlockNode, curItem);
            else
                processCallNode(callBlockNode, curItem);
        } else if (isExitNode(curItem.getICFGNodeWrapper())) {
            // Process FunExitICFGNode
            //
            // Generate function summary and re-analyze
            // the callsites which can consume the summary
            if (PSAOptions::PathSensitive() && PSAOptions::EnableIsoSummary())
                processExitNodeIso(curItem);
            else
                processExitNode(curItem);
        } else if (isBranchNode(curItem.getICFGNodeWrapper())) {
            // Process Branch Node
            //
            // Early terminate for infeasible branch
            processBranchNode(curItem);

        } else {
            // Process Other Node
            //
            // Update execution and abstract state
            // according to the next ICFG node
            processOtherNode(curItem);
        }
    }
}

/*!
 * Process CallICFGNode
 *
 * Generate function summary, apply summary if summary is generated already
 * @param callBlockNode
 * @param wlItem
 */
void PSTA::processCallNode(const CallICFGNode *callBlockNode, WLItem &curItem) {
    FunctionSet funSet;
    callee(callBlockNode, funSet);
    for (const auto &fun: funSet) {
        SymState symState = SVFUtil::move(getSymStateIn(curItem));
        for (const auto &absState: getFSMParser()->getAbsStates()) {
            SymState summary = getSummary(fun, symState.getAbstractState(), absState);
            // Apply summary if summary is not empty
            if (!summary.isNullSymState()) {
                // Process formal out to actual out
                nonBranchFlowFun(curItem.getICFGNodeWrapper()->getRetICFGNodeWrapper(), summary);
                for (const auto &outEdge: getOutTEdges(nextNodeToAdd(curItem.getICFGNodeWrapper()))) {
                    if (addInfo(outEdge, curItem.getTypeState(), summary)) {
                        _workList.push(
                                WLItem(outEdge->getDstNode(), curItem.getTypeState(), summary.getAbstractState()));
                    }
                }
            }
        }
        // Process actual in to formal in
        nonBranchFlowFun(curItem.getICFGNodeWrapper(), symState);
        const ICFGNodeWrapper *entry = entryNode(fun);
        for (const auto &entryE: entry->getOutEdges()) {
            if (addTrigger(entryE, symState)) {
                _workList.push(WLItem(entryE->getDstNode(), symState.getAbstractState(), symState.getAbstractState()));
            }
        }
    }
}

/*!
 * Process CallICFGNode using isolated summary
 *
 * Generate function summary, apply summary if summary is generated already
 * @param callBlockNode
 * @param wlItem
 */
void PSTA::processCallNodeIso(const CallICFGNode *callBlockNode, WLItem &curItem) {
    // add summary and add trigger for callee (Lines 15-18)
    FunctionSet funSet;
    callee(callBlockNode, funSet);
    const SVFVarVector &actualParamList = callBlockNode->getActualParms();
    for (const auto &fun: funSet) {
        SymState symState = SVFUtil::move(getSymStateIn(curItem));
        for (const auto &absState: getFSMParser()->getAbsStates()) {
            SymState summary = getSummary(fun, symState.getAbstractState(), absState);
            // Apply summary if summary is not empty
            if (!summary.isNullSymState()) {
                nonBranchFlowFun(curItem.getICFGNodeWrapper()->getRetICFGNodeWrapper(), summary);
                SymState newSymState = SVFUtil::move(applySummary(symState, summary));
                for (const auto &outEdge: getOutTEdges(nextNodeToAdd(curItem.getICFGNodeWrapper()))) {
                    if (addInfo(outEdge, curItem.getTypeState(), newSymState)) {
                        _workList.push(
                                WLItem(outEdge->getDstNode(), curItem.getTypeState(), newSymState.getAbstractState()));
                    }
                }
            }
        }
        nonBranchFlowFun(curItem.getICFGNodeWrapper(), symState);
        const ICFGNodeWrapper *entry = entryNode(fun);
        const auto *entryNode = SVFUtil::dyn_cast<FunEntryICFGNode>(entry->getICFGNode());
        Set<const FormalINSVFGNode *> formalIns = getFormalInSVFGNodes(entryNode);
        buildTrigger(symState.getExecutionState(), entryNode->getFormalParms(), formalIns);
//        if (PSAOptions::EnableReport()) {
//            symState.clearKeyNodesSet();
//            symState.setBranchCondition(Z3Expr::getTrueCond());
//        }
        for (const auto &entryE: entry->getOutEdges()) {
            if (addTriggerIso(entryE, symState)) {
                _workList.push(WLItem(entryE->getDstNode(), symState.getAbstractState(),
                                      symState.getAbstractState()));
            }
        }
    }
}

/*!
 * Process FunExitICFGNode
 *
 * Generate function summary and re-analyze
 * the callsites which can consume the summary
 * @param curItem
 */
void PSTA::processExitNode(WLItem &curItem) {
    SymState symState = SVFUtil::move(getSymStateIn(curItem));
    nonBranchFlowFun(curItem.getICFGNodeWrapper(), symState);
    if (addToSummary(curItem.getICFGNodeWrapper(), curItem.getTypeState(), symState)) {
        std::vector<const ICFGNodeWrapper *> retNodes;
        returnSites(curItem.getICFGNodeWrapper(), retNodes);
        for (const auto &retSite: retNodes) {
            for (const auto &formalInAs: getFSMParser()->getAbsStates()) {
                for (const auto &inEdge: getInTEdges(callSite(retSite))) {
                    for (const auto &callerAs: getFSMParser()->getAbsStates()) {
                        if (!getInfo(inEdge, callerAs, formalInAs).isNullSymState()) {
                            if (hasSummary(fn(curItem.getICFGNodeWrapper()), formalInAs)) {
                                _workList.push(WLItem(callSite(retSite), callerAs, formalInAs));
                            }
                        }
                    }
                }
            }
        }
    }
}

/*!
 * Process FunExitICFGNode using isolated summary
 *
 * Generate function summary and re-analyze
 * the callsites which can consume the summary
 * @param curItem
 */
void PSTA::processExitNodeIso(WLItem &curItem) {
    SymState symState = SVFUtil::move(getSymStateIn(curItem));
    nonBranchFlowFun(curItem.getICFGNodeWrapper(), symState);
    if (addToSummaryIso(curItem.getICFGNodeWrapper(), curItem.getTypeState(), std::move(symState))) {
        std::vector<const ICFGNodeWrapper *> retNodes;
        returnSites(curItem.getICFGNodeWrapper(), retNodes);
        for (const auto &retSite: retNodes) {
            for (const auto &formalInAs: getFSMParser()->getAbsStates()) {
                for (const auto &inEdge: getInTEdges(callSite(retSite))) {
                    for (const auto &callerAs: getFSMParser()->getAbsStates()) {
                        if (!getInfo(inEdge, callerAs, formalInAs).isNullSymState()) {
                            if (hasSummary(fn(curItem.getICFGNodeWrapper()), formalInAs)) {
                                _workList.push(WLItem(callSite(retSite), callerAs, formalInAs));
                            }
                        }
                    }
                }
            }
        }
    }
}

/*!
 * Process Branch Node
 *
 * Early terminate for infeasible branch
 * @param wlItem
 */
void PSTA::processBranchNode(WLItem &wlItem) {
    for (const auto &edge: wlItem.getICFGNodeWrapper()->getOutEdges()) {
        if (const IntraCFGEdge *intraCfgEdge = SVFUtil::dyn_cast<IntraCFGEdge>(edge->getICFGEdge())) {
            if (intraCfgEdge->getCondition()) {
                SymState symState = SVFUtil::move(getSymStateIn(wlItem));
                PC_TYPE brCond = evalBranchCond(intraCfgEdge);
                branchFlowFun(symState, intraCfgEdge, brCond);
                TypeState curAbsState = symState.getAbstractState();
                if (addInfo(edge, wlItem.getTypeState(), SVFUtil::move(symState))) {
                    _workList.push(WLItem(edge->getDstNode(), wlItem.getTypeState(), curAbsState));
                }
            }
        }
    }
}

/*!
 * Process Other Node
 *
 * Update execution and abstract state
 * according to the next ICFG node
 * @param wlItem
 */
void PSTA::processOtherNode(WLItem &wlItem) {
    SymState symState = SVFUtil::move(getSymStateIn(wlItem));
    nonBranchFlowFun(wlItem.getICFGNodeWrapper(), symState);
    for (const auto &outEdge: getOutTEdges(nextNodeToAdd(wlItem.getICFGNodeWrapper()))) {
        if (addInfo(outEdge, wlItem.getTypeState(), symState)) {
            _workList.push(WLItem(outEdge->getDstNode(), wlItem.getTypeState(), symState.getAbstractState()));
        }
    }
}

/*!
 * Get the symstates from in edges
 * @param curItem
 * @param symStatesIn
 */
SymState PSTA::getSymStateIn(WLItem &curItem) {
    if (isMergeNode(curItem.getICFGNodeWrapper())) {
        return SVFUtil::move(
                mergeFlowFun(curItem.getICFGNodeWrapper(), curItem.getTypeState(), curItem.getIndexTypeState()));
    } else {
        assert(!curItem.getICFGNodeWrapper()->getInEdges().empty() && "in edge empty?");
        return getInfo(*curItem.getICFGNodeWrapper()->getInEdges().begin(),
                       curItem.getTypeState(), curItem.getIndexTypeState());
    }
}

void PSTA::printSS() {
    outs() << "------------------\n";
    outs() << "SVFGNode Node: " << _curEvalSVFGNode->getValue()->toString() << "\n";
    outs() << "------------------\n";
    outs() << "Info map:\n";
    for (const auto &it: _infoMap) {
        const ICFGEdgeWrapper *edge = it.first.first;
        outs() << "{" << std::to_string(edge->getSrcNode()->getId()) << "-->"
               << std::to_string(edge->getDstNode()->getId()) << ", ";
        outs() << TypeStateParser::toString(it.first.second) << "}: \n";
        for (const auto &it2: it.second) {
            outs() << "\t[" << TypeStateParser::toString(it2.first) << ",";
            it2.second.getExecutionState().printExprValues();
            outs() << "]";
        }
        outs() << "\n";

    }
    if (!PSAOptions::EnableIsoSummary()) {
        outs() << "**********************************\n";
        outs() << "**********************************\n";
        outs() << "Summary map:\n";
        for (const auto &it: _summaryMap) {
            const SVFFunction *fun = it.first.first;
            outs() << "{" << fun->getName() << ", " << TypeStateParser::toString(it.first.second) << "}: \n";
            for (const auto &it2: it.second) {
                outs() << "\t[" << TypeStateParser::toString(it2.first) << ",";
                it2.second.getExecutionState().printExprValues();
                outs() << "]";
            }
            outs() << "\n";
        }
    } else {
        outs() << "**********************************\n";
        outs() << "**********************************\n";
        outs() << "Summary map:\n";
        for (const auto &it: _summaryMap) {
            const SVFFunction *fun = it.first.first;
            outs() << "{" << fun->getName() << ", " << TypeStateParser::toString(it.first.second) << "}: \n";
            for (const auto &it2: it.second) {
                outs() << "\t[" << TypeStateParser::toString(it2.first) << ",";
                it2.second.getExecutionState().printExprValues();
                outs() << "]";
            }
            outs() << "\n";
        }
    }
}

/// Flow functions
//{%
/*!
 * Combine two dataflow facts into a single fact, using set union
 * @param icfgNodeWrapper
 * @param absState
 * @param symStatesOut
 */
SymState PSTA::mergeFlowFun(const ICFGNodeWrapper *icfgNodeWrapper, const TypeState &absState,
                            const TypeState &indexAbsState) {
    SymStates symStatesTmp;
    for (const auto &edge: icfgNodeWrapper->getInEdges()) {
        if (edge->getICFGEdge()->isIntraCFGEdge()) {
            const SymState &symState = getInfo(edge, absState, indexAbsState);
            if (!symState.isNullSymState())
                symStatesTmp.push_back(symState);
        }
    }
    SymState symStateOut;
    groupingAbsStates(symStatesTmp, symStateOut);
    return SVFUtil::move(symStateOut);
}

void PSTA::performStat(string model) {
    _stat->performStat(SVFUtil::move(model));
}



