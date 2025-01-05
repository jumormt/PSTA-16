//
// Created by Xiao on 2023/7/1.
//

#include "PSTA/PSTABase.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "PSTA/Logger.h"
#include "PSTA/BTPExtractor.h"

#include <queue>

using namespace SVF;
using namespace SVFUtil;
using namespace std;

ExeStateManager *ExeStateManager::_exeStateMgr = nullptr;

ExeStateManager::~ExeStateManager() {
    delete _globalES;
    _globalES = nullptr;
}

void ExeStateManager::moveToGlobal() {
    translator.moveToGlobal();
}

void ExeStateManager::handleNonBranch(const ICFGNode *node) {
    if (!PSAOptions::PathSensitive()) return;
    for (const SVFStmt *stmt: node->getSVFStmts()) {
        if (const AddrStmt *addr = SVFUtil::dyn_cast<AddrStmt>(stmt)) {
            translator.translateAddr(addr);
        } else if (const BinaryOPStmt *binary = SVFUtil::dyn_cast<BinaryOPStmt>(stmt)) {
            translator.translateBinary(binary);
        } else if (const CmpStmt *cmp = SVFUtil::dyn_cast<CmpStmt>(stmt)) {
            translator.translateCmp(cmp);
        } else if (const UnaryOPStmt *unary = SVFUtil::dyn_cast<UnaryOPStmt>(stmt)) {
        } else if (const BranchStmt *br = SVFUtil::dyn_cast<BranchStmt>(stmt)) {
        } else if (const LoadStmt *load = SVFUtil::dyn_cast<LoadStmt>(stmt)) {
            translator.translateLoad(load);
        } else if (const StoreStmt *store = SVFUtil::dyn_cast<StoreStmt>(stmt)) {
            translator.translateStore(store);
        } else if (const CopyStmt *copy = SVFUtil::dyn_cast<CopyStmt>(stmt)) {
            translator.translateCopy(copy);
        } else if (const GepStmt *gep = SVFUtil::dyn_cast<GepStmt>(stmt)) {
            translator.translateGep(gep, isa<GlobalICFGNode>(node));
        } else if (const SelectStmt *select = SVFUtil::dyn_cast<SelectStmt>(stmt)) {
            translator.translateSelect(select);
        } else if (const PhiStmt *phi = SVFUtil::dyn_cast<PhiStmt>(stmt)) {
            translator.translatePhi(phi);
        } else if (const CallPE *callPE = SVFUtil::dyn_cast<CallPE>(stmt)) {
            translator.translateCall(callPE);
        } else if (const RetPE *retPE = SVFUtil::dyn_cast<RetPE>(stmt)) {
            translator.translateRet(retPE);
        } else {
            assert(false && "undefined statement");
        }
    }
    ICFG *icfg = PAG::getPAG()->getICFG();
    if (const CallICFGNode *callNode = dyn_cast<CallICFGNode>(node)) {
        if (SVFUtil::isExtCall(callNode->getCallSite())) {
            if (const SVFFunction *fun = SVFUtil::getCallee(callNode->getCallSite())) {
                if (fun->getName() == "globalReturnsTrueOrFalse") {
                    (*_es)[callNode->getRetICFGNode()->getActualRet()->getId()] = ConsExeState::getContext().int_const(
                            fun->getName().c_str());
                }
            }
        }
    }

}

bool ExeStateManager::handleBranch(const IntraCFGEdge *edge) {
    if (!PSAOptions::PathSensitive()) return true;
    assert(edge->getCondition() && "not a conditional control-flow transfer?");
    u32_t cond = PAG::getPAG()->getValueNode(edge->getCondition());
    if (!_es->inVarToValTable(cond))
        return true;
    SingleAbsValue &toboolExpr = (*_es)[cond];
    SingleAbsValue successorValExpr = edge->getSuccessorCondValue();
    SingleAbsValue condition = (toboolExpr == successorValExpr).simplify();
    if (condition.isBottom())
        return false;
    if (condition.isTop()) {
        return true;
    } else {
        const Z3Expr &curPc = _es->getBrCond();
        Z3Expr nPc = (curPc && condition.getExpr()).simplify();
        if (nPc.getExpr().is_true()) return true;
        if (nPc.getExpr().is_false()) return false;
        if (Z3Expr::getExprSize(nPc) > PSAOptions::MaxSymbolSize()) {
            nPc = Z3Expr::getTrueCond();
            _es->setBrCond(nPc);
            return true;
        }
        z3::check_result res = ConsExeState::solverCheck(nPc);
        if (res == z3::unsat) {
            return false;
        } else {
            if (!eq(nPc, curPc)) {
                _es->setBrCond(nPc);
                return true;
            } else
                return true;
        }
    }
}

ConsExeState ExeStateManager::nullExeState() {
    return ConsExeState::nullExeState();
}


ConsExeState &ExeStateManager::getOrBuildGlobalExeState(GlobalICFGNode *node) {
    if (_globalES == nullptr) {
        _globalES = new ConsExeState(getInitExeState());
        getExeStateMgr()->setEs(_globalES);
        getExeStateMgr()->handleNonBranch(node);
        getExeStateMgr()->moveToGlobal();
    }
    return *_globalES;
}

void ExeStateManager::handleGlobalNode() {
    if (ConsExeState::globalConsES == nullptr) {
        ConsExeState::globalConsES = new ConsExeState(ConsExeState::initExeState());
        ConsExeState tmpEs(ConsExeState::initExeState());
        getExeStateMgr()->setEs(&tmpEs);
        getExeStateMgr()->handleNonBranch(PAG::getPAG()->getICFG()->getGlobalICFGNode());
        getExeStateMgr()->moveToGlobal();
    }
}

ConsExeState ExeStateManager::extractGlobalExecutionState() {
    assert(_globalES && "global state not initialized?");
    if (PSAOptions::EnableSpatialSlicing()) {
        ConsExeState newEs(getInitExeState());
//        newEs.buildGlobES(*_globalES, vars);
        return SVFUtil::move(newEs);
    } else {
        return *_globalES;
    }
}

ConsExeState ExeStateManager::getInitExeState() {
    ConsExeState initExeState(ConsExeState::initExeState());
    return initExeState;
}

void ExeStateManager::collectGlobalStore() {
    for (const auto &item: ConsExeState::globalConsES->getVarToAddrs()) {
        for (const auto &addr: item.second) {
            assert(ConsExeState::isVirtualMemAddress(addr) && "must be addr");
            _globalStore.insert(ConsExeState::getInternalID(addr));
        }
    }
}

void ExeStateManager::collectFuncToGlobalPtrs(ICFG *icfg) {
    getOrBuildGlobalExeState(icfg->getGlobalICFGNode());
    collectGlobalStore();
    for (const auto &item: *icfg) {
        for (const auto &stmt: item.second->getSVFStmts()) {
            if (const StoreStmt *storeStmt = SVFUtil::dyn_cast<StoreStmt>(stmt)) {
                if (_globalStore.count(storeStmt->getLHSVarID())) {
                    if (item.second->getFun()) {
                        _funcToGlobalPtrs[item.second->getFun()].insert(storeStmt->getLHSVarID());
                    }
                }
            }
        }
    }
}


/*!
 * Flow function for processing branch node
 * @param intraEdge
 * @param pcType
 * @return
 */
bool SymStateManager::branchFlowFun(const IntraCFGEdge *intraEdge, PC_TYPE pcType) {
    if (PSAOptions::EnableReport()) {
        Z3Expr branchCond = BranchAllocator::getCondAllocator()->getBranchCond(intraEdge);
        _symState->setBranchCondition((_symState->getBranchCondition() && branchCond).simplify());
    }
    if (pcType == PC_TYPE::UNK_PC) {
        // Update execution state according to branch condition
        getExeStateMgr()->setEs(&_symState->getExecutionState());
        return getExeStateMgr()->handleBranch(intraEdge);
    } else {
        return pcType != PC_TYPE::FALSE_PC;
    }
}

/*!
 * Flow function for processing non-branch node
 * @param icfgNode
 * @param svfgNode
 */
void SymStateManager::nonBranchFlowFun(const ICFGNode *icfgNode, const SVFGNode *svfgNode) {
    // Update execution state according to ICFG node
    getExeStateMgr()->setEs(&_symState->getExecutionState());
    getExeStateMgr()->handleNonBranch(icfgNode);
    if (!PSAOptions::OTFAlias() || PSAOptions::LayerNum() != 0 || !PSAOptions::PathSensitive()) {
        preNonBranchFlowFun(icfgNode, svfgNode);
    } else {
        onTheFlyNonBranchFlowFun(icfgNode, svfgNode);
    }
}

void SymStateManager::preNonBranchFlowFun(const SVF::ICFGNode *icfgNode, const SVF::SVFGNode *svfgNode) {
    auto it2 = getAbsTransitionHandler()->getICFGAbsTransferMap().find(icfgNode);
    if (it2 != getAbsTransitionHandler()->getICFGAbsTransferMap().end()) {
        // Update abstract state
        auto it3 = it2->second.find(_symState->getAbstractState());
        if (it3->second != _symState->getAbstractState()) {
            if (PSAOptions::EnableReport())
                _symState->insertKeyNode(icfgNode->getId());
            _symState->setAbsState(it3->second);
        }
        return;
    }
}

void SymStateManager::onTheFlyNonBranchFlowFun(const SVF::ICFGNode *icfgNode, const SVFGNode *svfgNode) {
    if (icfgNode == _curEvalICFGNode) {
        if (const CallICFGNode *callNode = dyn_cast<CallICFGNode>(_curEvalICFGNode)) {
            if (_symState->getExecutionState().inVarToAddrsTable(callNode->getRetICFGNode()->getActualRet()->getId())) {
                const Addrs &vaddrs =
                        _symState->getExecutionState().getAddrs(callNode->getRetICFGNode()->getActualRet()->getId());
                if (vaddrs.size() == 1) _srcNum = *vaddrs.begin();
            }

        }
        const TypeState &tgtAbsState = getAbsTransitionHandler()->absStateTransition(
                _symState->getAbstractState(),
                getFSMParser()->getSrcAction());
        if (tgtAbsState != _symState->getAbstractState()) {
            if (PSAOptions::EnableReport())
                _symState->insertKeyNode(icfgNode->getId());
            _symState->setAbsState(tgtAbsState);
        }
        return;
    } // end src node
    FSMAction fsmAction;
    if (const CallICFGNode *callNode = isNonSrcFSMCallNode(icfgNode, fsmAction)) {
        NodeID paramId = (*callNode->getActualParms().begin())->getId();
        if (_srcNum == 0 || !_symState->getExecutionState().inVarToAddrsTable(paramId)) {
            preNonBranchFlowFun(icfgNode, svfgNode);
            return;
        }
        if (_src != 0 && _symState->getExecutionState().getAddrs(paramId).contains(_srcNum)) {
            const TypeState &tgtAbsState = getAbsTransitionHandler()->absStateTransition(
                    _symState->getAbstractState(), fsmAction);
            if (tgtAbsState != _symState->getAbstractState()) {
                if (PSAOptions::EnableReport())
                    _symState->insertKeyNode(icfgNode->getId());
                _symState->setAbsState(tgtAbsState);
            }
            return;
        }
    } else if (const IntraICFGNode *intraICFGNode = dyn_cast<IntraICFGNode>(icfgNode)) {
        if (PSAOptions::LoadAsUse() && !PSAOptions::Wrapper() &&
            _checkerTypes.find(FSMParser::CK_USE) != _checkerTypes.end()) {
            std::list<const SVFStmt *> svfStmts = intraICFGNode->getSVFStmts();
            if (!svfStmts.empty()) {
                for (const auto &svfStmt: svfStmts) {
                    if (_srcNum == 0 || !_symState->getExecutionState().inVarToAddrsTable(svfStmt->getDstID())) {
                        preNonBranchFlowFun(icfgNode, svfgNode);
                        return;
                    }
                    if (isa<LoadStmt>(svfStmt) && _src != 0 &&
                        _symState->getExecutionState().getAddrs(svfStmt->getDstID()).contains(_srcNum)) {
                        const TypeState &tgtAbsState = getAbsTransitionHandler()->absStateTransition(
                                _symState->getAbstractState(), FSMAction::CK_USE);
                        if (tgtAbsState != _symState->getAbstractState()) {
                            if (PSAOptions::EnableReport())
                                _symState->insertKeyNode(icfgNode->getId());
                            _symState->setAbsState(tgtAbsState);
                        }
                        return;
                    }
                }
            }
        } // end CK_USE
        if (_checkerTypes.find(FSMParser::CK_RET) != _checkerTypes.end()) {
            if (intraICFGNode->getFun() == getAbsTransitionHandler()->getMainFunc() &&
                intraICFGNode->getInst()->isRetInst()) {
                const TypeState &tgtAbsState = getAbsTransitionHandler()->absStateTransition(
                        _symState->getAbstractState(), FSMAction::CK_RET);
                if (tgtAbsState != _symState->getAbstractState()) {
                    if (PSAOptions::EnableReport())
                        _symState->insertKeyNode(icfgNode->getId());
                    _symState->setAbsState(tgtAbsState);
                }
                return;
            }
        } // end CK_RET
    }
}

/*!
 * Grouping function (property simulation)
 *
 * @param symStates the first element in symStates is the original symState
 * @param symStatesOut the resulting grouped symstate
 */
bool SymStateManager::groupingAbsStates(const SymStates &symStates, SymState &symStateOut) {
    if (symStates.size() == 1) {
        symStateOut = std::move(const_cast<SymState &>(*symStates.begin()));
        return false;
    }
    bool changed = false;
    ConsExeState tmpExeState(ExeStateManager::nullExeState());
    KeyNodesSet keyNodesSet;
    Z3Expr brc;
    for (auto &it: symStates) {
        if (tmpExeState.isNullState()) {
            tmpExeState = SVFUtil::move(const_cast<ConsExeState &>(it.getExecutionState()));
            if (PSAOptions::EnableReport()) {
                for (const auto &n: it.getKeyNodesSet()) {
                    keyNodesSet.insert(std::move(const_cast<KeyNodes &>(n)));
                }
                brc = it.getBranchCondition();
            }
        } else {
            if (tmpExeState.joinWith(it.getExecutionState())) {
                changed = true;
            }
            if (PSAOptions::EnableReport()) {
                for (const auto &n: it.getKeyNodesSet()) {
                    keyNodesSet.insert(std::move(const_cast<KeyNodes &>(n)));
                }
                brc = (brc || it.getBranchCondition()).simplify();
            }
        }
    }
    SymState sOut(SVFUtil::move(tmpExeState), symStates.begin()->getAbstractState());
    if (PSAOptions::EnableReport()) {
        sOut.setBranchCondition(brc);
        sOut.setKeyNodesSet(SVFUtil::move(keyNodesSet));
    }
    symStateOut = std::move(sOut);
    return changed;
}

/*!
 * Whether pre is a super/equal set of nxt
 */
bool SymStateManager::isSupEqSymStates(const SymStates &pre, const SymStates &nxt) {
    if (pre.size() < nxt.size())
        return false;
    AbsStateToSymStateRefMap preMap;
    mapAbsStateToSymStateRef(pre, preMap);
    AbsStateToSymStateRefMap nxtMap;
    mapAbsStateToSymStateRef(nxt, nxtMap);

    for (const auto &nxtItem: nxtMap) {
        auto it = preMap.find(nxtItem.first);
        if (it == preMap.end()) {
            return false;
        } else {
            if (*it->second != *nxtItem.second)
                return false;
        }
    }
    return true;

}

void SymStateManager::setCurEvalICFGNode(const SVF::ICFGNode *icfgNode) {
    _curEvalICFGNode = icfgNode;
    const CallICFGNode *callNode = dyn_cast<CallICFGNode>(_curEvalICFGNode);
    _src = callNode->getRetICFGNode()->getActualRet()->getId();
}


PSTABase::PSTABase() {
    _emptySymState = new SymState();
    _fsmFile = PSAOptions::FSMFILE();
}

PSTABase::~PSTABase() {
    _summaryMap.clear();
    _infoMap.clear();
    ExeStateManager::releaseExeStateManager();
    delete _emptySymState;
    _emptySymState = nullptr;
    AndersenWaveDiff::releaseAndersenWaveDiff();
    SVFIR::releaseSVFIR();
    Z3Expr::releaseContext();
    Logger::releaseLogger();
}

Logger *Logger::DefaultLogger;
LogLevel Logger::Level;
std::string Logger::TraceFilename;

void initLogger() {
    std::string level = PSAOptions::LogLevel();
    if (PSAOptions::DumpState()) {
        string fullName(SymbolTableInfo::SymbolInfo()->getModule()->getModuleIdentifier());
        string name = fullName.substr(fullName.find('/'), fullName.size());
        string fileName = name.substr(0, fullName.find('.'));
        if (PSAOptions::EnableIsoSummary())
            fileName.append("_ISO");
        if (PSAOptions::MultiSlicing()) {
            fileName.append("_Mul");
            if (PSAOptions::EnableTemporalSlicing()) {
                fileName.append("_T");
            }
        }
        if (PSAOptions::EnableSpatialSlicing()) {
            fileName.append("_S");
        }
        fileName.append("_" + std::to_string(PSAOptions::LayerNum()));
        fileName.append(".log");
        fileName = PSAOptions::OUTPUT() + fileName;
        Logger::TraceFilename = fileName;
    }

    if (level == "error")
        Logger::Level = LogLevel::Error;
    else if (level == "warning")
        Logger::Level = LogLevel::Warning;
    else if (level == "info")
        Logger::Level = LogLevel::Info;
    else if (level == "debug")
        Logger::Level = LogLevel::Debug;
}


/*!
 * Initialization
 * @param module
 * @param _fsmFile
 */
void PSTABase::initialize(SVFModule *module) {

    initLogger();

    // Init SVF-related models
    SVFIRBuilder builder(module);
    // build pag (program assignment graph)
    SVFIR *svfir = builder.build();
    // run Andersen analysis
    AndersenWaveDiff *ander = AndersenWaveDiff::createAndersenWaveDiff(svfir);
    // get icfg
    _icfg = svfir->getICFG();
    // indirect call function, e.g., function pointer
    _ptaCallgraph = ander->getPTACallGraph();
    // update icfg
    _icfg->updateCallGraph(_ptaCallgraph);
    // Init FSM parser based on file
    FSMParser::createFSMParser(_fsmFile);
    // Init entry of main function
    // find the main function entry and save it in _mainEntry and _mainFunc
    getAbsTransitionHandler()->initMainFunc(module);
    _mainFunc = getAbsTransitionHandler()->getMainFunc();
    _mainEntry = _icfg->getFunEntryICFGNode(_mainFunc);
    for (const auto &func: *module) {
        if (!SVFUtil::isExtCall(func)) {
            // Allocate conditions for a program.
            for (SVFFunction::const_iterator bit = func->begin(), ebit = func->end();
                 bit != ebit; ++bit) {
                const SVFBasicBlock *bb = *bit;
                collectBBCallingProgExit(*bb);
            }
        }
    }
    assert(_mainEntry && "no main function?");
    // Init sources and save to _srcs
    getAbsTransitionHandler()->initSrcs(_srcs);
//    getExeStateMgr()->getOrBuildGlobalExeState(_icfg->getGlobalICFGNode());
    getExeStateMgr()->handleGlobalNode();
    getExeStateMgr()->collectGlobalStore();
}

void PSTABase::initHandler(SVFModule *module) {
    ICFG *icfg = PAG::getPAG()->getICFG();
}

/*!
 * Initialize info and summary map
 * @param module svf module
 */
void PSTABase::initMap(SVFModule *module) {
    _infoMap.clear();
    _summaryMap.clear();
}

/*!
 * Analyzing entry
 * @param module
 * @param _fsmFile
 */
void PSTABase::analyze(SVFModule *module) {
    initialize(module);
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


        _symStateMgr.setCurEvalICFGNode(_curEvalICFGNode);
        _symStateMgr.setCurEvalFuns(_curEvalFuns);

        initHandler(module);

        initMap(module);
        solve();

        reportBug();
    }
}

/*!
 * Main algorithm
 */
void PSTABase::solve() {
    SymState initSymState(ConsExeState::initExeState(),
                          getFSMParser()->getUninitAbsState());
    ICFGNode *mainEntryNode = _icfg->getICFGNode(_mainEntry->getId());
    if (mainEntryNode->getOutEdges().empty()) return;
    for (const auto &e: mainEntryNode->getOutEdges()) {
        addInfo(e, getFSMParser()->getUninitAbsState(), initSymState);
        ESPWLItem firstItem(e->getDstNode(), getFSMParser()->getUninitAbsState(), getFSMParser()->getUninitAbsState());
        _workList.push(firstItem);
    }
    while (!_workList.empty()) {
        ESPWLItem curItem = _workList.pop();
        // Process CallICFGNode
        //
        // Generate function summary, apply summary
        // if summary is generated already
        if (const CallICFGNode *callBlockNode = isCallNode(curItem.getICFGNode())) {
            processCallNode(callBlockNode, curItem);
        } else if (isExitNode(curItem.getICFGNode())) {
            // Process FunExitICFGNode
            //
            // Generate function summary and re-analyze
            // the callsites which can consume the summary
            processExitNode(curItem);
        } else if (isBranchNode(curItem.getICFGNode())) {
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
void PSTABase::processCallNode(const CallICFGNode *callBlockNode, ESPWLItem &curItem) {
    FunctionSet funSet;
    callee(callBlockNode, funSet);
    for (const auto &fun: funSet) {
        SymState symState = SVFUtil::move(getSymStateIn(curItem));
        for (const auto &absState: getFSMParser()->getAbsStates()) {
            SymState summary = getSummary(fun, symState.getAbstractState(), absState);
            // Apply summary if summary is not empty
            if (!summary.isNullSymState()) {
                // Process formal out to actual out
                nonBranchFlowFun(dyn_cast<CallICFGNode>(curItem.getICFGNode())->getRetICFGNode(), summary);
                for (const auto &outEdge: getOutTEdges(nextNodeToAdd(curItem.getICFGNode()))) {
                    if (addInfo(outEdge, curItem.getTypeState(), summary)) {
                        _workList.push(
                                ESPWLItem(outEdge->getDstNode(), curItem.getTypeState(), summary.getAbstractState()));
                    }
                }
            }
        }
        // Process actual in to formal in
        nonBranchFlowFun(curItem.getICFGNode(), symState);
        const ICFGNode *entry = entryNode(fun);
        for (const auto &entryE: entry->getOutEdges()) {
            if (addTrigger(entryE, symState)) {
                _workList.push(
                        ESPWLItem(entryE->getDstNode(), symState.getAbstractState(), symState.getAbstractState()));
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
void PSTABase::processExitNode(ESPWLItem &curItem) {
    SymState symState = SVFUtil::move(getSymStateIn(curItem));
    nonBranchFlowFun(curItem.getICFGNode(), symState);
    if (addToSummary(curItem.getICFGNode(), curItem.getTypeState(), symState)) {
        std::vector<const ICFGNode *> retNodes;
        returnSites(curItem.getICFGNode(), retNodes);
        for (const auto &retSite: retNodes) {
            for (const auto &formalInAs: getFSMParser()->getAbsStates()) {
                for (const auto &inEdge: getInTEdges(callSite(retSite))) {
                    for (const auto &callerAs: getFSMParser()->getAbsStates()) {
                        if (!getInfo(inEdge, callerAs, formalInAs).isNullSymState()) {
                            if (hasSummary(fn(curItem.getICFGNode()), formalInAs)) {
                                _workList.push(ESPWLItem(callSite(retSite), callerAs, formalInAs));
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
void PSTABase::processBranchNode(ESPWLItem &wlItem) {
    for (const auto &edge: wlItem.getICFGNode()->getOutEdges()) {
        if (const IntraCFGEdge *intraCfgEdge = SVFUtil::dyn_cast<IntraCFGEdge>(edge)) {
            if (intraCfgEdge->getCondition()) {
                SymState symState = SVFUtil::move(getSymStateIn(wlItem));
                PC_TYPE brCond = evalBranchCond(intraCfgEdge);
                branchFlowFun(symState, intraCfgEdge, brCond);
                TypeState curAbsState = symState.getAbstractState();
                if (addInfo(edge, wlItem.getTypeState(), SVFUtil::move(symState))) {
                    _workList.push(ESPWLItem(edge->getDstNode(), wlItem.getTypeState(), curAbsState));
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
void PSTABase::processOtherNode(ESPWLItem &wlItem) {
    SymState symState = SVFUtil::move(getSymStateIn(wlItem));
    nonBranchFlowFun(wlItem.getICFGNode(), symState);
    for (const auto &outEdge: getOutTEdges(nextNodeToAdd(wlItem.getICFGNode()))) {
        if (addInfo(outEdge, wlItem.getTypeState(), symState)) {
            _workList.push(ESPWLItem(outEdge->getDstNode(), wlItem.getTypeState(), symState.getAbstractState()));
        }
    }
}

/*!
 * Get the symstates from in edges
 * @param curItem
 * @param symStatesIn
 */
SymState PSTABase::getSymStateIn(ESPWLItem &curItem) {
    if (isMergeNode(curItem.getICFGNode())) {
        return SVFUtil::move(
                mergeFlowFun(curItem.getICFGNode(), curItem.getTypeState(), curItem.getIndexTypeState()));
    } else {
        assert(!curItem.getICFGNode()->getInEdges().empty() && "in edge empty?");
        return getInfo(*curItem.getICFGNode()->getInEdges().begin(),
                       curItem.getTypeState(), curItem.getIndexTypeState());
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
SymState PSTABase::mergeFlowFun(const ICFGNode *icfgNode, const TypeState &absState,
                                const TypeState &indexAbsState) {
    SymStates symStatesTmp;
    for (const auto &edge: icfgNode->getInEdges()) {
        if (edge->isIntraCFGEdge()) {
            const SymState &symState = getInfo(edge, absState, indexAbsState);
            if (!symState.isNullSymState())
                symStatesTmp.push_back(symState);
        }
    }
    SymState symStateOut;
    groupingAbsStates(symStatesTmp, symStateOut);
    return SVFUtil::move(symStateOut);
}


bool PSTABase::isTestContainsNullAndTheValue(const CmpStmt *cmp) {
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
PSTABase::PC_TYPE PSTABase::evaluateTestNullLikeExpr(const BranchStmt *cmpInst, const IntraCFGEdge *edge) {
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

/*!
 * Evaluate loop exit branch to be true if
 * bb is loop header and succ is the only exit basic block outside the loop (excluding exit bbs which call program exit)
 * for all other case, we conservatively evaluate false for now
 */
PSTABase::PC_TYPE PSTABase::evaluateLoopExitBranch(const SVFBasicBlock *bb, const SVFBasicBlock *dst) {
    const SVFFunction *svffun = bb->getParent();
    assert(svffun == dst->getParent() && "two basic blocks should be in the same function");

    if (svffun->isLoopHeader(bb)) {
        Set<const SVFBasicBlock *> filteredbbs;
        std::vector<const SVFBasicBlock *> exitbbs;
        svffun->getExitBlocksOfLoop(bb, exitbbs);
        /// exclude exit bb which calls program exit
        for (const SVFBasicBlock *eb: exitbbs) {
            if (!isBBCallsProgExit(eb))
                filteredbbs.insert(eb);
        }

        /// if the dst dominate all other loop exit bbs, then dst can certainly be reached
        bool allPDT = true;
        for (const auto &filteredbb: filteredbbs) {
            if (!postDominate(dst, filteredbb))
                allPDT = false;
        }

        if (allPDT)
            return PC_TYPE::TRUE_PC;
    }
    return PC_TYPE::UNK_PC;
}

/*!
 * Evaluate condition for program exit (e.g., exit(0))
 */
PSTABase::PC_TYPE PSTABase::evaluateProgExit(const BranchStmt *brInst, const SVFBasicBlock *succ) {
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

PSTABase::PC_TYPE PSTABase::evalBranchCond(const IntraCFGEdge *intraCfgEdge) {
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
void PSTABase::collectBBCallingProgExit(const SVFBasicBlock &bb) {

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
bool PSTABase::isBBCallsProgExit(const SVFBasicBlock *bb) {
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

//void PSTABase::performStat(string model) {
//    _stat->performStat(SVFUtil::move(model));
//}

void PSTABase::buildTrigger(ConsExeState &es, const SVFVarVector &formalVs, Set<const FormalINSVFGNode *> &formalIns) {
    ConsExeState newEs(ConsExeState::initExeState());

    for (const auto &formalIn: formalIns) {
        for (const auto &id: formalIn->getPointsTo()) {
            if (es.inLocalLocToValTable(id)) {
                newEs.store(SingleAbsValue(getVirtualMemAddress(id)),
                            es.load(SingleAbsValue(getVirtualMemAddress(id))));
            } else if (es.inLocalLocToAddrsTable(id)) {
                newEs.storeAddrs(getVirtualMemAddress(id), es.loadAddrs(getVirtualMemAddress(id)));
            }
        }
    }
    for (const auto &param: formalVs) {
        if (es.inVarToValTable(param->getId())) {
            newEs[param->getId()] = es[param->getId()];
        } else if (es.inVarToAddrsTable(param->getId())) {
            newEs.getAddrs(param->getId()) = es.getAddrs(param->getId());
        }
    }
    es = SVFUtil::move(newEs);
}

/*!
 * Build summary based on side-effects and formal return
 *
 * @param summary
 * @param v
 * @param formalRet
 */
void PSTABase::buildSummary(ConsExeState &es, ConsExeState &summary, const SVFVar *formalRet,
                            Set<const FormalOUTSVFGNode *> &formalOuts) {
    if (formalRet) {
        if (summary.inVarToValTable(formalRet->getId())) {
            es[formalRet->getId()] = summary[formalRet->getId()];
        } else if (summary.inVarToAddrsTable(formalRet->getId())) {
            es.getAddrs(formalRet->getId()) = summary.getAddrs(formalRet->getId());
        }
    }

    for (const auto &formalOut: formalOuts) {
        for (const auto &id: formalOut->getPointsTo()) {
            if (summary.inLocalLocToValTable(id)) {
                es.store(SingleAbsValue(getVirtualMemAddress(id)),
                         summary.load(SingleAbsValue(getVirtualMemAddress(id))));
            } else if (summary.inLocalLocToAddrsTable(id)) {
                es.storeAddrs(getVirtualMemAddress(id), summary.loadAddrs(getVirtualMemAddress(id)));
            }
        }
    }
}

bool PSTABase::isSrcSnkReachable(const ConsExeState &src, const ConsExeState &snk) {
    const Z3Expr &snkBrCond = snk.getBrCond();
    const Z3Expr &srcBrCond = src.getBrCond();
    const Z3Expr &jointBrCond = (srcBrCond && snkBrCond).simplify();
    if (jointBrCond.getExpr().is_false()) return false;
    if (jointBrCond.getExpr().is_true() || Z3Expr::getExprSize(jointBrCond) > PSAOptions::MaxSymbolSize()) return true;
    return ConsExeState::solverCheck(jointBrCond) != z3::unsat;
}
