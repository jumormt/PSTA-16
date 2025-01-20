//
// Created by Xiao on 7/31/2022.
//
#include "PSTA/BTPExtractor.h"
#include "Slicing/ControlDGBuilder.h"
#include "PSTA/Logger.h"
#include <cmath>

using namespace SVF;
using namespace SVFUtil;

/*!
 * Provide detailed bug report
 * @param branchCond
 * @param keyNodesSet
 * @param curEvalICFGNode
 * @param curEvalSVFGNode
 * @param mainEntry
 */
void
BTPExtractor::detailBugReport(const Z3Expr &branchCond, const KeyNodesSet &keyNodesSet, const ICFGNode *curEvalICFGNode,
                              const SVFGNode *curEvalSVFGNode, const ICFGNode *mainEntry,
                              Set<const SVFFunction *> &curEvalFuns) {
    Log(LogLevel::Info) << "Generating detailed bug report...";
    Dump() << "Generating detailed bug report...";
    ICFG *icfg = PAG::getPAG()->getICFG();
    std::string branchStr;
    std::stringstream rawBranchStr(branchStr);
    BranchAllocator *condAllocator = BranchAllocator::getCondAllocator();
    NodeBS elems = condAllocator->exactCondElem(branchCond);
    Set<std::string> locations;
    Map<const ICFGNode *, Set<const ICFGEdge *>> conditionalEdges;
    // Record the branches the error state passes
    for (u32_t elem: elems) {
        std::pair<const SVFInstruction *, const SVFInstruction *> tinstP = condAllocator->getCondInst(elem);
        locations.insert(tinstP.first->getSourceLoc() + " --> " + tinstP.second->getSourceLoc());
        if (const ICFGEdge *edge = condAllocator->getConditionalEdge(elem)) {
            conditionalEdges[edge->getSrcNode()].insert(edge);
        }
    }
    for (const auto &location: locations) {
//        rawstr << "\t\t  --> (" << *iter << ") \n";
        rawBranchStr << "\t\t\t\t(" << location << ") \n";
    }

    std::string str;
    std::stringstream rawstr(str);
    // Record the locations where abstract value changes
    std::vector<std::string> keyNodesLocations;
    u32_t ct = 0;
    for (const auto &keyNodes: keyNodesSet) {
        // Produce bug report and bug-triggering paths
        Set<u32_t> temporalSlice, spatialSlice, callSites;
        ICFGWrapperBuilder builder;
        builder.build(icfg);
        removeFSMNodeBody(curEvalICFGNode, curEvalFuns);
        temporalSlicing(curEvalSVFGNode, keyNodes,
                        getICFGWrapper()->getICFGNodeWrapper(mainEntry->getId()), temporalSlice, curEvalFuns);
        spatialSlicing(curEvalSVFGNode, keyNodes, temporalSlice, spatialSlice, callSites);
        getICFGWrapper()->annotateMulSlice(temporalSlice, spatialSlice, callSites, keyNodes);
        std::string fileName = std::to_string(curEvalICFGNode->getId()) + "_" + std::to_string(ct++);
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
        // Maintain conditional edges, which may be pruned by slicing (mem leak)
        if (PSAOptions::LEAK()) {
            for (const auto &item: conditionalEdges) {
                for (const auto &e: item.second) {
                    getICFGWrapper()->getICFGNodeWrapper(e->getSrcID())->_inTSlice = true;
                    getICFGWrapper()->getICFGNodeWrapper(e->getSrcID())->_inSSlice = true;
                    getICFGWrapper()->getICFGNodeWrapper(e->getDstID())->_inTSlice = true;
                    getICFGWrapper()->getICFGNodeWrapper(e->getDstID())->_inSSlice = true;
                }
            }
        }
        std::string wpPath = "WP-";
        getICFGWrapper()->dump(PSAOptions::OUTPUT() + wpPath);
        getICFGWrapper()->removeFilledColor();
        removeConditionalEdge(conditionalEdges);
        extractBTPs(curEvalICFGNode, curEvalFuns, mainEntry);
        std::string btpPath = "BTP-";
        getICFGWrapper()->dump(PSAOptions::OUTPUT() + btpPath);

        for (u32_t keyNode: keyNodes) {
            const ICFGNode *icfgNode = icfg->getICFGNode(keyNode);
            if (const CallICFGNode *callICFGNode = SVFUtil::dyn_cast<CallICFGNode>(icfgNode)) {
                keyNodesLocations.push_back(SVFUtil::move(callICFGNode->getCallSite()->getSourceLoc()));
            } else if (const IntraICFGNode *intraIcfgNode = SVFUtil::dyn_cast<IntraICFGNode>(icfgNode)) {
                keyNodesLocations.push_back(SVFUtil::move(intraIcfgNode->getInst()->getSourceLoc()));
            } else {
                assert(false && "not a call node or intra node?");
            }
        }
        keyNodesLocations.emplace_back("------------------------------------");
    }
    for (const auto &keyNodesLocation: keyNodesLocations) {
        rawstr << "\t\t\t\t(" << keyNodesLocation << ") \n";
    }
    SVFUtil::errs() << SVFUtil::errMsg("\n\t\t Key nodes: \n") << rawstr.str();

    SVFUtil::errs() << SVFUtil::errMsg("\t\t Passing branches: \n") << rawBranchStr.str() << "\n";

}

/*!
 * Remove the conditional edge that does not hold (used for bug report)
 * @param conditionalEdges
 */
void BTPExtractor::removeConditionalEdge(Map<const ICFGNode *, Set<const ICFGEdge *>> &conditionalEdges) {
    Set<ICFGEdgeWrapper *> edgeToRM;
    for (const auto &item: *getICFGWrapper()) {
        auto it = conditionalEdges.find(item.second->getICFGNode());
        if (it != conditionalEdges.end()) {
            for (const auto &e: item.second->getOutEdges()) {
                if (!it->second.count(e->getICFGEdge())) {
                    edgeToRM.insert(e);
                }
            }
        }
    }
    for (auto &e: edgeToRM) {
        getICFGWrapper()->removeICFGEdgeWrapper(e);
    }
}

/*!
 * Extracting bug-triggering paths
 * @param curEvalICFGNode
 * @param mainEntry
 */
void BTPExtractor::extractBTPs(const ICFGNode *curEvalICFGNode, Set<const SVFFunction *> &curEvalFuns,
                               const ICFGNode *mainEntry) {
    // Remove irrelevant ICFG nodes
    Set<ICFGNodeWrapper *> callNodesToCompact, intraNodesToCompact, nodesToRemove;
    getICFGWrapper()->getICFGNodeWrapper(mainEntry->getId())->_inTSlice = true;
    getICFGWrapper()->getICFGNodeWrapper(mainEntry->getId())->_inSSlice = true;

    for (const auto &item: *getICFGWrapper()) {

        // Spatial slicing is enabled
        // Remove callsites if spatial slicing is enabled
        if (isCallNode(item.second, curEvalICFGNode, curEvalFuns)) {
            if (!item.second->_inTSlice) {
                nodesToRemove.insert(item.second);
                nodesToRemove.insert(item.second->getRetICFGNodeWrapper());
            } else if (!item.second->_validCS)
                callNodesToCompact.insert(item.second);
        } else if (isCallNode(item.second->getCallICFGNodeWrapper(), curEvalICFGNode, curEvalFuns)) {
            // Skip return node
            continue;
        } else if (isFSMCallNode(item.second, curEvalICFGNode, curEvalFuns)) {
            // Temporal and Spatial Slicing enabled
            if (!item.second->_inTSlice) {
                nodesToRemove.insert(item.second);
                nodesToRemove.insert(item.second->getRetICFGNodeWrapper());
            } else if (!item.second->_inSSlice) {
                callNodesToCompact.insert(item.second);
            }
        } else if (isFSMCallNode(item.second->getCallICFGNodeWrapper(), curEvalICFGNode, curEvalFuns)) {
            continue;
        } else if (isExtCall(item.second)) {
            // Temporal and Spatial Slicing enabled
            if (!item.second->_inTSlice && !item.second->getRetICFGNodeWrapper()->_inTSlice) {
                nodesToRemove.insert(item.second);
                nodesToRemove.insert(item.second->getRetICFGNodeWrapper());
            } else if (!item.second->_inSSlice && !item.second->getRetICFGNodeWrapper()->_inSSlice) {
                callNodesToCompact.insert(item.second);
            }
        } else if (isExtCall(item.second->getCallICFGNodeWrapper())) {
            continue;
        } else {
            // Non-callsites node
            // Temporal and Spatial Slicing enabled
            if (!item.second->_inTSlice) {
                nodesToRemove.insert(item.second);
            } else if (!item.second->_inSSlice) {
                intraNodesToCompact.insert(item.second);
            }
        }
    }

    for (const auto &node: nodesToRemove) {
        if (node) getICFGWrapper()->removeICFGNodeWrapper(node);
    }
    for (const auto &node: callNodesToCompact) {
        compactCallNodes(node);
    }

    for (const auto &node: intraNodesToCompact) {
        compactIntraNodes(node);
    }
}


/*!
 * Single temporal slicing
 *
 * @param curEvalSVFGNode
 * @param keyNodes
 * @param mainEntry
 * @param temporalSlice
 */
void
BTPExtractor::temporalSlicing(const SVFGNode *curEvalSVFGNode, const KeyNodes &keyNodes,
                              const ICFGNodeWrapper *mainEntry,
                              Set<u32_t> &temporalSlice, Set<const SVFFunction *> &curEvalFuns) {
    clearDF();
    std::vector<PIState::DataFact> allDataFacts;
    KeyNodes dataFactTmp;
    for (const auto &n: keyNodes) {
        if (const CallICFGNode *callNode = dyn_cast<CallICFGNode>(
                getICFGWrapper()->getICFGNodeWrapper(n)->getICFGNode())) {
            dataFactTmp.push_back(callNode->getRetICFGNode()->getId());
        } else {
            dataFactTmp.push_back(n);
        }
    }
    PIState::DataFact dataFact(dataFactTmp.rbegin(), dataFactTmp.rend());
    while (!dataFact.empty()) {
        allDataFacts.push_back(dataFact);
        dataFact.pop_back();
    }
    // zero data fact
    allDataFacts.emplace_back();
    initBUDFTransferFunc(mainEntry, allDataFacts);
    buIFDSSolve(mainEntry, getICFGWrapper()->getICFGNodeWrapper(dataFactTmp.back()), curEvalSVFGNode->getICFGNode(), curEvalFuns, allDataFacts);
    // early terminate - no seq at entry
    if ((*mainEntry->getOutEdges().begin())->getDstNode()->_buReachableDataFacts.find(allDataFacts[0]) ==
        (*mainEntry->getOutEdges().begin())->getDstNode()->_buReachableDataFacts.end())
        return;
    initTDDFTransferFunc(mainEntry, allDataFacts);
    tdIFDSSolve(mainEntry, curEvalSVFGNode->getICFGNode(), curEvalFuns, allDataFacts);

    for (const auto &n: *getICFGWrapper()) {
        if (!n.second->_tdReachableDataFacts.empty() && !n.second->_buReachableDataFacts.empty()) {
            for (const auto &df1: n.second->_tdReachableDataFacts) {
                // has non-zero intersection datafacts
                if (!df1.empty() &&
                    n.second->_buReachableDataFacts.find(df1) != n.second->_buReachableDataFacts.end()) {
                    temporalSlice.insert(n.first);
                }
            }
        }
    }
}

/*!
 * Single spatial slicing
 * @param src
 * @param keyNodes
 * @param temporalSlice
 * @param spatialSlice
 * @param callsites
 */
void
BTPExtractor::spatialSlicing(const SVFGNode *src, const KeyNodes &keyNodes, Set<u32_t> &temporalSlice,
                             Set<u32_t> &spatialSlice, Set<u32_t> &callsites) {
    ControlDGBuilder cdBuilder;
    cdBuilder.build();
    ControlDG *controlDG = ControlDG::getControlDG();
    PAG *pag = PAG::getPAG();
    ICFG *icfg = pag->getICFG();

    AndersenWaveDiff *ander = AndersenWaveDiff::createAndersenWaveDiff(pag);
    SVFGBuilder svfgBuilder;
    SVFG *svfg = svfgBuilder.buildFullSVFG(ander);
    Set<CxtDPItem> callGDPItems;

    Set<CxtDPItem> workListLayer;
    Set<u32_t> visited;
    getICFGWrapper()->annotateTemporalSlice(temporalSlice);
    ContextCond cxt;
    for (const auto &id: keyNodes) {
        const ICFGNodeWrapper *icfgNodeWrapper = getICFGWrapper()->getICFGNodeWrapper(id);
        CxtDPItem item(id, cxt);
        if (icfgNodeWrapper->_inTSlice) {
            visited.insert(item.getCurNodeID());
            workListLayer.insert(SVFUtil::move(item));
            spatialSlice.insert(id);
            if (const SVFFunction *func = icfgNodeWrapper->getICFGNode()->getFun()) {
                callGDPItems.insert(
                        SVFUtil::move(CxtDPItem(getPTACallGraph()->getCallGraphNode(func)->getId(), cxt)));
            }
        }
    }

    int layerNum = PSAOptions::LayerNum();
    while (!workListLayer.empty() && (PSAOptions::LayerNum() == 0 || layerNum-- > 0)) {
        Set<CxtDPItem> nxtWorkListLayer;
        for (const auto &curNode: workListLayer) {
            NodeID curNodeID = curNode.getCurNodeID();
            if (const ControlDGNode *cdNode = controlDG->getControlDGNode(curNodeID)) {
                for (const auto &e: cdNode->getInEdges()) {
                    if (getICFGWrapper()->getICFGNodeWrapper(e->getSrcID())->_inTSlice) {
                        CxtDPItem item(e->getSrcID(), curNode.getContexts());
                        if (visited.find(item.getCurNodeID()) == visited.end()) {
                            visited.insert(item.getCurNodeID());
                            nxtWorkListLayer.insert(SVFUtil::move(item));
                            spatialSlice.insert(e->getSrcID());
                        }
                    }
                }
            }

            const ICFGNodeWrapper *icfgNodeWrapper = getICFGWrapper()->getICFGNodeWrapper(curNodeID);
            if (const RetICFGNode *retICFGNode = dyn_cast<RetICFGNode>(icfgNodeWrapper->getICFGNode())) {
                if (icfgNodeWrapper->_inTSlice) {
                    if (spatialSlice.find(retICFGNode->getCallICFGNode()->getId()) == spatialSlice.end()) {
                        spatialSlice.insert(retICFGNode->getCallICFGNode()->getId());
                    }
                }

                const CallICFGNode *callNode = retICFGNode->getCallICFGNode();
                if (SVFUtil::isExtCall(callNode->getCallSite())) {
                    // handle external call
                    NodeID lhsId = pag->getValueNode(callNode->getCallSite());
                    CallSite cs(callNode->getCallSite());
                    for (u32_t i = 0; i < cs.getNumArgOperands(); i++) {
                        const SVFGNode *vfNode = svfg->getDefSVFGNode(
                                pag->getGNode(pag->getValueNode(cs.getArgOperand(i))));
                        NodeID nodeId = vfNode->getICFGNode()->getId();
                        CxtDPItem item(nodeId, curNode.getContexts());
                        if (visited.find(item.getCurNodeID()) == visited.end()) {
                            visited.insert(item.getCurNodeID());
                            spatialSlice.insert(nodeId);
                            nxtWorkListLayer.insert(SVFUtil::move(item));
                        }
                    }
                }
            }

            // data dependence
            ICFGNode::SVFStmtList svfStmtList = icfgNodeWrapper->getICFGNode()->getSVFStmts();
            if (!svfStmtList.empty()) {
                const SVFStmt *svfStmt = *svfStmtList.begin();
                if (const BranchStmt *branchStmt = dyn_cast<BranchStmt>(svfStmt)) {
                    const SVFGNode *vfNode = svfg->getDefSVFGNode(branchStmt->getCondition());
                    NodeID nodeId = vfNode->getICFGNode()->getId();
                    if (getICFGWrapper()->getICFGNodeWrapper(nodeId)->_inTSlice) {
                        CxtDPItem item(nodeId, curNode.getContexts());
                        if (visited.find(item.getCurNodeID()) == visited.end()) {
                            spatialSlice.insert(nodeId);
                            visited.insert(nodeId);
                            nxtWorkListLayer.insert(SVFUtil::move(item));
                        }
                    }
                } // end branch data slicing
                if (const GepStmt *gepStmt = dyn_cast<GepStmt>(svfStmt)) {
                    for (int i = gepStmt->getOffsetVarAndGepTypePairVec().size() - 1; i >= 0; i--) {
                        const SVFVar *offsetVar = gepStmt->getOffsetVarAndGepTypePairVec()[i].first;

                        const SVFConstantInt *op = SVFUtil::dyn_cast<SVFConstantInt>(offsetVar->getValue());
                        if (!op) {
                            /// variable as the offset
                            NodeID index = offsetVar->getId();
                            const SVFGNode *vfNode = svfg->getDefSVFGNode(PAG::getPAG()->getGNode(index));
                            NodeID nodeId = vfNode->getICFGNode()->getId();
                            if (getICFGWrapper()->getICFGNodeWrapper(nodeId)->_inTSlice) {
                                CxtDPItem item(nodeId, curNode.getContexts());
                                if (visited.find(item.getCurNodeID()) == visited.end()) {
                                    spatialSlice.insert(nodeId);
                                    visited.insert(nodeId);
                                    nxtWorkListLayer.insert(SVFUtil::move(item));
                                }
                            }
                        }
                    }
                } // end gep data slicing
            } // end special intra data slicing
            const ICFGNode::VFGNodeList &vfNodes = icfgNodeWrapper->getICFGNode()->getVFGNodes();
            for (const auto &vfNode: vfNodes) {
                for (const auto &vEdge: vfNode->getInEdges()) {
                    VFGNode *srcVFNode = vEdge->getSrcNode();
                    const ICFGNode *srcCFNode = srcVFNode->getICFGNode();
                    CxtDPItem newItem(srcCFNode->getId(), curNode.getContexts());
                    if (vEdge->isRetVFGEdge()) {
                        CallSiteID csId = 0;
                        if (const RetDirSVFGEdge *retEdge = SVFUtil::dyn_cast<RetDirSVFGEdge>(vEdge))
                            csId = retEdge->getCallSiteId();
                        else
                            csId = SVFUtil::cast<RetIndSVFGEdge>(vEdge)->getCallSiteId();
                        newItem.pushContext(csId);
                    } else if (vEdge->isCallVFGEdge()) {
                        CallSiteID csId = 0;
                        if (const CallDirSVFGEdge *callEdge = SVFUtil::dyn_cast<CallDirSVFGEdge>(vEdge))
                            csId = callEdge->getCallSiteId();
                        else
                            csId = SVFUtil::cast<CallIndSVFGEdge>(vEdge)->getCallSiteId();

                        if (!newItem.matchContext(csId)) continue;
                    }
                    /// whether this dstNode has been visited or not
                    if (visited.count(newItem.getCurNodeID())) continue;
                    else visited.insert(newItem.getCurNodeID());
                    spatialSlice.insert(srcCFNode->getId());
                    if (const SVFFunction *fun = srcCFNode->getFun()) {
                        NodeID callGraphId = getPTACallGraph()->getCallGraphNode(fun)->getId();
                        callGDPItems.insert(SVFUtil::move(CxtDPItem(callGraphId, newItem.getContexts())));
                    }

                    if (isa<IntraMSSAPHISVFGNode>(srcVFNode)) {
                        for (const auto &vEdge2: srcVFNode->getInEdges()) {
                            VFGNode *srcVFNode2 = vEdge2->getSrcNode();
                            const ICFGNode *srcCFNode2 = srcVFNode->getICFGNode();
                            CxtDPItem newItem2(srcCFNode2->getId(), newItem.getContexts());
                            if (vEdge2->isRetVFGEdge()) {
                                CallSiteID csId = 0;
                                if (const RetDirSVFGEdge *retEdge = SVFUtil::dyn_cast<RetDirSVFGEdge>(vEdge2))
                                    csId = retEdge->getCallSiteId();
                                else
                                    csId = SVFUtil::cast<RetIndSVFGEdge>(vEdge)->getCallSiteId();
                                newItem2.pushContext(csId);
                            } else if (vEdge2->isCallVFGEdge()) {
                                CallSiteID csId = 0;
                                if (const CallDirSVFGEdge *callEdge = SVFUtil::dyn_cast<CallDirSVFGEdge>(
                                        vEdge2))
                                    csId = callEdge->getCallSiteId();
                                else
                                    csId = SVFUtil::cast<CallIndSVFGEdge>(vEdge2)->getCallSiteId();

                                if (!newItem2.matchContext(csId)) continue;
                            }
                            if (visited.count(newItem2.getCurNodeID())) continue;
                            else visited.insert(newItem2.getCurNodeID());
                            spatialSlice.insert(srcCFNode2->getId());
                            if (const SVFFunction *fun = srcCFNode2->getFun()) {
                                NodeID callGraphId = getPTACallGraph()->getCallGraphNode(fun)->getId();
                                callGDPItems.insert(
                                        SVFUtil::move(CxtDPItem(callGraphId, newItem2.getContexts())));
                            }
                            nxtWorkListLayer.insert(SVFUtil::move(newItem2));
                        }
                    }
                    nxtWorkListLayer.insert(SVFUtil::move(newItem));
                }
                if (SVFUtil::isa<ActualOUTSVFGNode>(vfNode) || SVFUtil::isa<ActualRetVFGNode>(vfNode)) {
                    for (const auto &actualin: FSMHandler::getAbsTransitionHandler()->getReachableActualInsOfActualOut(
                            vfNode)) {
                        if (PSAOptions::EnableTemporalSlicing() &&
                            !getICFGWrapper()->getICFGNodeWrapper(actualin->getICFGNode()->getId())->_inTSlice)
                            continue;
                        CxtDPItem newItem(actualin->getICFGNode()->getId(), curNode.getContexts());
                        if (visited.count(newItem.getCurNodeID())) continue;
                        else visited.insert(newItem.getCurNodeID());
                        spatialSlice.insert(actualin->getICFGNode()->getId());
                        if (const SVFFunction *fun = actualin->getICFGNode()->getFun()) {
                            NodeID callGraphId = getPTACallGraph()->getCallGraphNode(fun)->getId();
                            CxtDPItem item(callGraphId, newItem.getContexts());
                            callGDPItems.insert(SVFUtil::move(item));
                        }
                        nxtWorkListLayer.insert(SVFUtil::move(newItem));
                    }
                    if (const RetICFGNode *retNode = SVFUtil::dyn_cast<RetICFGNode>(vfNode->getICFGNode())) {
                        const CallICFGNode *callNode = retNode->getCallICFGNode();
                        if (PSAOptions::EnableTemporalSlicing() &&
                            !getICFGWrapper()->getICFGNodeWrapper(callNode->getId())->_inTSlice)
                            return;
                        if (const SVFFunction *fun = SVFUtil::getCallee(callNode->getCallSite())) {
                            NodeID callGraphId = getPTACallGraph()->getCallGraphNode(fun)->getId();
                            ContextCond curContext = curNode.getContexts();
                            curContext.pushContext(getPTACallGraph()->getCallSiteID(callNode, fun));
                            CxtDPItem item(callGraphId, curContext);
                            callGDPItems.insert(SVFUtil::move(item));
                        }
                    }

                }
            }

        }
        workListLayer = SVFUtil::move(nxtWorkListLayer);
    }

    Set<CxtDPItem> callVisited;
    FIFOWorkList<CxtDPItem> workList;
    for (const auto &item: callGDPItems) {
        workList.push(item);
        callVisited.insert(item);
    }
    while (!workList.empty()) {
        CxtDPItem curItem = workList.pop();
        for (const auto &e: getPTACallGraph()->getCallGraphNode(curItem.getCurNodeID())->getInEdges()) {
            CxtDPItem newItem(e->getSrcID(), curItem.getContexts());
            if (!newItem.matchContext(e->getCallSiteID())) continue;
            callsites.insert(getPTACallGraph()->getCallSite(e->getCallSiteID())->getId());
            if (callVisited.count(newItem)) continue;
            else callVisited.insert(newItem);
            workList.push(SVFUtil::move(newItem));
        }
    }

}


BranchAllocator *BranchAllocator::_condAllocator = nullptr;

BranchAllocator::BranchAllocator() : _totalCondNum(0) {

}

/*!
 * Allocate path condition for each branch
 */
void BranchAllocator::allocate() {
    DBOUT(DGENERAL, outs() << pasMsg("path condition allocation starts\n"));

    for (const auto &it: *PAG::getPAG()->getICFG())
        allocateForICFGNode(it.second);

    if (PSAOptions::PrintPathCond())
        printPathCond();

    DBOUT(DGENERAL, outs() << pasMsg("path condition allocation ends\n"));
}

/*!
 * Allocate conditions for an ICFGNode and propagate its condition to its successors.
 */
void BranchAllocator::allocateForICFGNode(const ICFGNode *icfgNode) {

    u32_t succ_number = 0;
    for (const auto &it: icfgNode->getOutEdges()) {
        if (const IntraCFGEdge *intraCFGEdge = SVFUtil::dyn_cast<IntraCFGEdge>(it)) {
            if (intraCFGEdge->getCondition())
                ++succ_number;
        }
    }

    // if successor number greater than 1, allocate new decision variable for successors
    if (succ_number > 1) {

        //allocate log2(num_succ) decision variables
        double num = log(succ_number) / log(2);
        auto bit_num = (u32_t) ceil(num);
        u32_t succ_index = 0;
        std::vector<Condition> condVec;
        for (u32_t i = 0; i < bit_num; i++) {
            const Condition &expr = newCond(icfgNode->getBB()->getTerminator());
            condVec.push_back(expr);
        }

        for (const auto &it: icfgNode->getOutEdges()) {
            if (SVFUtil::isa<IntraCFGEdge>(it)) {
                Condition path_cond = getTrueCond();
                ///TODO: handle BranchInst and SwitchInst individually here!!

                // for each successor decide its bit representation
                // decide whether each bit of succ_index is 1 or 0, if (three successor) succ_index is 000 then use C1^C2^C3
                // if 001 use C1^C2^negC3
                for (u32_t j = 0; j < bit_num; j++) {
                    //test each bit of this successor's index (binary representation)
                    u32_t tool = 0x01 << j;
                    if (tool & succ_index)
                        path_cond = condAnd(path_cond, (condNeg(condVec.at(j))));
                    else
                        path_cond = condAnd(path_cond, condVec.at(j));
                }
                setBranchCond(it, path_cond);
                ++succ_index;
            }
        }
    }
}

/*!
 * Get a branch condition
 */
BranchAllocator::Condition &BranchAllocator::getBranchCond(const ICFGEdge *edge) const {
    const IntraCFGEdge *intraCfgEdge = dyn_cast<IntraCFGEdge>(edge);
    assert(intraCfgEdge && "not intra cfg edge?");
    assert(intraCfgEdge->getCondition() && "no condition on intra edge?");
    auto it = icfgNodeConds.find(edge->getSrcNode());
    assert(it != icfgNodeConds.end() && "icfg Node does not have branch and conditions??");
    CondPosMap condPosMap = it->second;
    s64_t pos = intraCfgEdge->getSuccessorCondValue();
    auto it2 = condPosMap.find(pos);
    assert(it2 != condPosMap.end() && "pos not in control condition map!");
    return it2->second;
}


/*!
 * Set a branch condition
 */
void BranchAllocator::setBranchCond(const ICFGEdge *edge, Condition &cond) {
    const IntraCFGEdge *intraCfgEdge = dyn_cast<IntraCFGEdge>(edge);
    assert(intraCfgEdge->getCondition() && "no condition on intra edge?");
    /// we only care about basic blocks have more than one successor
    assert(intraCfgEdge->getSrcNode()->getBB()->getNumSuccessors() > 1 && "not more than one successor??");
    s64_t pos = intraCfgEdge->getSuccessorCondValue();
    CondPosMap &condPosMap = icfgNodeConds[edge->getSrcNode()];

    /// FIXME: llvm getNumSuccessors allows duplicated block in the successors, it makes this assertion fail
    /// In this case we may waste a condition allocation, because the overwrite of the previous cond
    //assert(condPosMap.find(pos) == condPosMap.end() && "this branch has already been set ");

    condPosMap[pos] = cond;
    auto it = _condIdToTermInstMap.find(cond.id());
    if (it != _condIdToTermInstMap.end()) {
        const ICFGNode *pNode = edge->getDstNode();
        const SVFInstruction *pInstruction = nullptr;
        if (const CallICFGNode *callNode = SVFUtil::dyn_cast<CallICFGNode>(pNode)) {
            pInstruction = callNode->getCallSite();
        } else if (const IntraICFGNode *intraNode = SVFUtil::dyn_cast<IntraICFGNode>(pNode)) {
            pInstruction = intraNode->getInst();
        } else {
            assert(false && "not callnode or intra node");
        }
        _condIdToTermInstMap[cond.id()] = std::make_pair(it->second.first, pInstruction);
    }
    _condIdToEdge[cond.id()] = edge;
}


/*!
 * Print path conditions
 */
void BranchAllocator::printPathCond() {

    outs() << "print path condition\n";

    for (const auto &icfgCond: icfgNodeConds) {
        const ICFGNode *icfgNode = icfgCond.first;
        for (const auto &cit: icfgCond.second) {
            u32_t i = 0;
            for (const ICFGEdge *edge: icfgNode->getOutEdges()) {
                if (i == cit.first) {
                    const Condition &cond = cit.second;
                    outs() << icfgNode->toString() << "-->" << edge->getDstNode()->toString() << ":";
                    outs() << dumpCond(cond) << "\n";
                    break;
                }
                i++;
            }
        }
    }
}

Z3Expr BranchAllocator::condAndLimit(const Z3Expr &lhs, const Z3Expr &rhs) {
    if (eq(lhs, getFalseCond()) || eq(rhs, getFalseCond()))
        return getFalseCond();
    else if (eq(lhs, getTrueCond()))
        return rhs;
    else if (eq(rhs, getTrueCond()))
        return lhs;
    else {
        const Z3Expr &expr = (lhs && rhs).simplify();
        // Widening if expression is too large in size
        if (getExprSize(expr) > PSAOptions::MaxBoolNum()) {
            z3::check_result result = solverCheck(expr);
            if (result != z3::unsat) {
                return getTrueCond();
            } else {
                return getFalseCond();
            }
        } else
            return expr;
    }
}

Z3Expr BranchAllocator::condOrLimit(const Z3Expr &lhs, const Z3Expr &rhs) {
    if (eq(lhs, getTrueCond()) || eq(rhs, getTrueCond()))
        return getTrueCond();
    else if (eq(lhs, getFalseCond()))
        return rhs;
    else if (eq(rhs, getFalseCond()))
        return lhs;
    else {
        // Widening if expression is too large in size
        const Z3Expr &expr = (lhs || rhs).simplify();
        if (getExprSize(expr) > PSAOptions::MaxBoolNum()) {
            z3::check_result result = solverCheck(expr);
            if (result != z3::unsat) {
                return getTrueCond();
            } else {
                return getFalseCond();
            }
        } else
            return expr;
    }
}

/*!
 * Size of the expression
 * @param z3Expr
 * @return
 */
u32_t BranchAllocator::getExprSize(const Z3Expr &z3Expr) {
    u32_t res = 1;
    if (z3Expr.getExpr().num_args() == 0) {
        return 1;
    }
    for (u32_t i = 0; i < z3Expr.getExpr().num_args(); ++i) {
        Z3Expr expr = z3Expr.getExpr().arg(i);
        res += getExprSize(expr);
    }
    return res;
}