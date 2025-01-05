//
// Created by Xiao on 7/24/2022.
//

#include "Slicing/SpatialSlicer.h"
#include "Slicing/ControlDGBuilder.h"
#include "PSTA/PSAOptions.h"
#include "SABER/SaberSVFGBuilder.h"
#include "PSTA/Logger.h"

using namespace SVF;
using namespace SVFUtil;


/*!
 * Spatial slicing
 * @param srcs
 */
void SpatialSlicer::spatialSlicing(const SVFGNode *src, ICFGNodeSet &snks) {
    _curEvalICFGNode = src->getICFGNode();
    if (const RetICFGNode *retICFGNOde = dyn_cast<RetICFGNode>(_curEvalICFGNode)) {
        _curEvalICFGNode = retICFGNOde->getCallICFGNode();
    }
    for (const auto &e: _curEvalICFGNode->getOutEdges()) {
        if (const CallCFGEdge *callEdge = dyn_cast<CallCFGEdge>(e)) {
            _curEvalFuns.insert(callEdge->getDstNode()->getFun());
        }
    }
    Log(LogLevel::Info) << "Extracting spatial slice...";
    Dump() << "Extracting spatial slice...";
    ControlDGBuilder cdBuilder;
    cdBuilder.build();
    ControlDG *controlDG = ControlDG::getControlDG();
    PAG *pag = PAG::getPAG();
    ICFG *icfg = pag->getICFG();

    if (!_svfg) {
        AndersenWaveDiff *ander = AndersenWaveDiff::createAndersenWaveDiff(pag);
        _svfg = _svfgBuilder.buildFullSVFG(ander);
        if (PSAOptions::CxtSensitiveSpatialSlicing()) getAbsTransitionHandler()->buildOutToIns(_svfg);
    }
    Set<u32_t> branch;
    Set<CxtDPItem> visitedCallGDPItems;
    Set<CxtDPItem> visitedCallSites;
    Set<u32_t> globalVars;
    Set<u32_t> visitedVFNodes;
    auto &icfgTransferFunc = const_cast<ICFGAbsTransitionFunc &>(getAbsTransitionHandler()->getICFGAbsTransferMap());
    if (PSAOptions::MultiSlicing() && PSAOptions::EnableTemporalSlicing())
        getICFGWrapper()->annotateTemporalSlice(_temporalSlice);
    std::vector<CxtDPItem> workListLayer;
    Set<u32_t> visited;
    ContextCond cxt;
    if (!PSAOptions::SSlicingNorm() && PSAOptions::MultiSlicing()) {
        for (const auto &df: _sQ) {
            for (const auto &id: df) {
                const ICFGNodeWrapper *icfgNodeWrapper = getICFGWrapper()->getICFGNodeWrapper(id);
                if (icfgNodeWrapper->getCallICFGNodeWrapper())
                    // add call node in spatial slice
                    icfgNodeWrapper = icfgNodeWrapper->getCallICFGNodeWrapper();
                _spatialSlice.insert(icfgNodeWrapper->getId());
                for (const auto &vfNode: icfgNodeWrapper->getICFGNode()->getVFGNodes()) {
                    if (!_svfg->hasSVFGNode(vfNode->getId())) continue;
                    if (SVFUtil::isa<MSSAPHISVFGNode>(vfNode)) continue;
                    CxtDPItem item(vfNode->getId(), cxt);
                    if (!PSAOptions::EnableTemporalSlicing() || icfgNodeWrapper->_inTSlice) {
                        visited.insert(item.getCurNodeID());
                        workListLayer.push_back(SVFUtil::move(item));
                        if (const SVFFunction *func = icfgNodeWrapper->getICFGNode()->getFun()) {
                            _cGDpItems.insert(
                                    SVFUtil::move(
                                            CxtDPItem(getPTACallGraph()->getCallGraphNode(func)->getId(), cxt)));
                        }
                    }
                }
            }
        } // end initialize
    } else {
        initLayerAndNs(workListLayer, icfgTransferFunc, snks);
    }
    visitedCallGDPItems.insert(_cGDpItems.begin(), _cGDpItems.end());
    int layerNum = PSAOptions::LayerNum();
    while (!workListLayer.empty() && (PSAOptions::LayerNum() == 0 || layerNum-- > 0)) {
        std::vector<CxtDPItem> nxtWorkListLayer;
        for (const auto &curNode: workListLayer) {
            u32_t curNodeID = curNode.getCurNodeID();
            _curSVFGNode = _svfg->getSVFGNode(curNodeID);
            NodeID cfNodeID = _curSVFGNode->getICFGNode()->getId();
            _curICFGNode = getICFGWrapper()->getICFGNodeWrapper(cfNodeID);
            controlSlicing(curNode, nxtWorkListLayer, visitedVFNodes, visitedCallSites, visited);
            extCallSlicing(curNode, nxtWorkListLayer, visitedVFNodes, visited);
//            gepSlicing(curNode, nxtWorkListLayer, visitedVFNodes, visited);
            if (!PSAOptions::EnableDataSlicing()) continue;
            dataSlicing(curNode, nxtWorkListLayer, visitedVFNodes, visitedCallGDPItems, visited);
        }
        workListLayer = SVFUtil::move(nxtWorkListLayer);
    }
    Log(LogLevel::Info) << SVFUtil::sucMsg("[done]\n");
    Dump() << SVFUtil::sucMsg("[done]\n");
}

void SpatialSlicer::initLayerAndNs(std::vector<CxtDPItem> &workListLayer, ICFGAbsTransitionFunc &icfgTransferFunc,
                                   ICFGNodeSet &snks) {
    ContextCond cxt;
    for (const auto &mpItem: icfgTransferFunc) {
        // have snkMap
        if (!snks.empty()) {
            if (!PSAOptions::Wrapper() && PSAOptions::LoadAsUse()) {
                // No wrapper
                if (const IntraICFGNode *intraICFGNode = dyn_cast<IntraICFGNode>(mpItem.first)) {
                    bool isLoad = false;
                    std::list<const SVFStmt *> svfStmts = intraICFGNode->getSVFStmts();
                    if (!svfStmts.empty()) {
                        for (const auto &svfStmt: svfStmts) {
                            if (isa<LoadStmt>(svfStmt)) {
                                isLoad = true;
                                break;
                            }
                        }
                    }
                    if (isLoad) {
                        // snk point
                        if (snks.count(mpItem.first)) {
                            if (!PSAOptions::EnableTemporalSlicing() ||
                                getICFGWrapper()->getICFGNodeWrapper(mpItem.first->getId())->_inTSlice) {
                                for (const auto &vfNode: mpItem.first->getVFGNodes()) {
                                    if(SVFUtil::isa<MSSAPHISVFGNode>(vfNode)) continue;
                                    if (_svfg->hasSVFGNode(vfNode->getId()))
                                        workListLayer.push_back(SVFUtil::move(CxtDPItem(vfNode->getId(), cxt)));
                                }
                                _spatialSlice.insert(mpItem.first->getId());
                                if (const SVFFunction *func = mpItem.first->getFun()) {
                                    _cGDpItems.insert(
                                            SVFUtil::move(
                                                    CxtDPItem(getPTACallGraph()->getCallGraphNode(func)->getId(),
                                                              cxt)));
                                }
                            }
                        }
                    } else {
                        if (!PSAOptions::EnableTemporalSlicing() ||
                            getICFGWrapper()->getICFGNodeWrapper(mpItem.first->getId())->_inTSlice) {
                            // non-snk point
                            for (const auto &vfNode: mpItem.first->getVFGNodes()) {
                                if(SVFUtil::isa<MSSAPHISVFGNode>(vfNode)) continue;
                                if (_svfg->hasSVFGNode(vfNode->getId()))
                                    workListLayer.push_back(SVFUtil::move(CxtDPItem(vfNode->getId(), cxt)));
                            }
                            _spatialSlice.insert(mpItem.first->getId());
                            if (const SVFFunction *func = mpItem.first->getFun()) {
                                _cGDpItems.insert(
                                        SVFUtil::move(
                                                CxtDPItem(getPTACallGraph()->getCallGraphNode(func)->getId(), cxt)));
                            }
                        }
                    }

                } else {
                    if (!PSAOptions::EnableTemporalSlicing() ||
                        getICFGWrapper()->getICFGNodeWrapper(mpItem.first->getId())->_inTSlice) {
                        // no wrapper && Not intra node
                        for (const auto &vfNode: mpItem.first->getVFGNodes()) {
                            if(SVFUtil::isa<MSSAPHISVFGNode>(vfNode)) continue;
                            if (_svfg->hasSVFGNode(vfNode->getId()))
                                workListLayer.push_back(SVFUtil::move(CxtDPItem(vfNode->getId(), cxt)));
                        }
                        _spatialSlice.insert(mpItem.first->getId());
                        if (const SVFFunction *func = mpItem.first->getFun()) {
                            _cGDpItems.insert(
                                    SVFUtil::move(CxtDPItem(getPTACallGraph()->getCallGraphNode(func)->getId(), cxt)));
                        }
                    }

                }
            } else {
                if (!PSAOptions::EnableTemporalSlicing() ||
                    getICFGWrapper()->getICFGNodeWrapper(mpItem.first->getId())->_inTSlice) {
                    if (const CallICFGNode *callBlockNode = SVFUtil::dyn_cast<CallICFGNode>(
                            mpItem.first)) {
                        Set<const SVFFunction *> functionSet;
                        getPTACallGraph()->getCallees(callBlockNode, functionSet);
                        if (functionSet.empty())
                            continue;
                        FSMParser::FSMAction action = getAbsTransitionHandler()->getTypeFromFunc((*functionSet.begin()));
                        if(snks.count(mpItem.first) || action != FSMParser::CK_USE) {
                            for (const auto &vfNode: mpItem.first->getVFGNodes()) {
                                if(SVFUtil::isa<MSSAPHISVFGNode>(vfNode)) continue;
                                if (_svfg->hasSVFGNode(vfNode->getId()))
                                    workListLayer.push_back(SVFUtil::move(CxtDPItem(vfNode->getId(), cxt)));
                            }
                            _spatialSlice.insert(mpItem.first->getId());
                            if (const SVFFunction *func = mpItem.first->getFun()) {
                                _cGDpItems.insert(
                                        SVFUtil::move(CxtDPItem(getPTACallGraph()->getCallGraphNode(func)->getId(), cxt)));
                            }
                        }
                    } else {
                        for (const auto &vfNode: mpItem.first->getVFGNodes()) {
                            if(SVFUtil::isa<MSSAPHISVFGNode>(vfNode)) continue;
                            if (_svfg->hasSVFGNode(vfNode->getId()))
                                workListLayer.push_back(SVFUtil::move(CxtDPItem(vfNode->getId(), cxt)));
                        }
                        _spatialSlice.insert(mpItem.first->getId());
                        if (const SVFFunction *func = mpItem.first->getFun()) {
                            _cGDpItems.insert(
                                    SVFUtil::move(CxtDPItem(getPTACallGraph()->getCallGraphNode(func)->getId(), cxt)));
                        }
                    }

                }
            }
        } else {
            if (!PSAOptions::EnableTemporalSlicing() ||
                getICFGWrapper()->getICFGNodeWrapper(mpItem.first->getId())->_inTSlice) {
                // no snkMap
                for (const auto &vfNode: mpItem.first->getVFGNodes()) {
                    if(SVFUtil::isa<MSSAPHISVFGNode>(vfNode)) continue;
                    if (_svfg->hasSVFGNode(vfNode->getId()))
                        workListLayer.push_back(SVFUtil::move(CxtDPItem(vfNode->getId(), cxt)));
                }
                _spatialSlice.insert(mpItem.first->getId());
                if (const SVFFunction *func = mpItem.first->getFun()) {
                    _cGDpItems.insert(
                            SVFUtil::move(CxtDPItem(getPTACallGraph()->getCallGraphNode(func)->getId(), cxt)));
                }
            }

        }
    }
}

/*!
 * Extract dependent vars in global node
 * @param node
 * @param globVars
 * @param visitedVFNodes
 */
void SpatialSlicer::extractGlobVars(const SVFGNode *node, Set<u32_t> &globVars, Set<u32_t> &visitedVFNodes) {
    if (!isa<GlobalICFGNode>(node->getICFGNode())) return;
    if (visitedVFNodes.count(node->getId())) return;
    visitedVFNodes.insert(node->getId());
    for (const auto &id: node->getDefSVFVars()) {
        globVars.insert(id);
    }
    for (const auto &e: node->getOutEdges()) {
        extractGlobVars(e->getDstNode(), globVars, visitedVFNodes);
    }
}

void SpatialSlicer::controlSlicing(const CxtDPItem &curNode, std::vector<CxtDPItem> &tmpLayer,
                                   Set<u32_t> &visitedVFNodes, Set<CxtDPItem> &visitedCallSites,
                                   Set<u32_t> &visited) {
    ControlDG *controlDG = ControlDG::getControlDG();
    if (const ControlDGNode *cdNode = controlDG->getControlDGNode(_curICFGNode->getId())) {
        for (const auto &e: cdNode->getInEdges()) {
            if (e->getSrcNode()->getICFGNode()->getSVFStmts().empty()) continue;
            if (const BranchStmt *branchStmt = dyn_cast<BranchStmt>(
                    *e->getSrcNode()->getICFGNode()->getSVFStmts().begin())) {
                _spatialSlice.insert(e->getSrcNode()->getId());
                const SVFGNode *vfNode = _svfg->getDefSVFGNode(branchStmt->getCondition());
                NodeID nodeId = vfNode->getId();
                if (!PSAOptions::EnableTemporalSlicing() ||
                    getICFGWrapper()->getICFGNodeWrapper(vfNode->getICFGNode()->getId())->_inTSlice) {
                    CxtDPItem item(nodeId, curNode.getContexts());
                    extractGlobVars(vfNode, _globVars, visitedVFNodes);
                    if (visited.find(item.getCurNodeID()) == visited.end()) {
                        visited.insert(item.getCurNodeID());
                        _spatialSlice.insert(vfNode->getICFGNode()->getId());
                        tmpLayer.push_back(SVFUtil::move(item));
                    }
                }
            } // end branch data slicing
        }
    } // end control slicing

    Set<u32_t> callSites;
    callsitesExtraction(_cGDpItems, callSites, visitedCallSites);
    _cGDpItems.clear();
    for (const auto &id: callSites) {
        if (_callsites.count(id)) continue;
        _callsites.insert(id);
        if (const ControlDGNode *cdNode = controlDG->getControlDGNode(id)) {
            for (const auto &e: cdNode->getInEdges()) {
                if (e->getSrcNode()->getICFGNode()->getSVFStmts().empty()) continue;
                if (const BranchStmt *branchStmt = dyn_cast<BranchStmt>(
                        *e->getSrcNode()->getICFGNode()->getSVFStmts().begin())) {
                    _spatialSlice.insert(e->getSrcNode()->getId());
                    const SVFGNode *vfNode = _svfg->getDefSVFGNode(branchStmt->getCondition());
                    NodeID nodeId = vfNode->getId();
                    if (!PSAOptions::EnableTemporalSlicing() ||
                        getICFGWrapper()->getICFGNodeWrapper(vfNode->getICFGNode()->getId())->_inTSlice) {
                        CxtDPItem item(nodeId, curNode.getContexts());
                        extractGlobVars(vfNode, _globVars, visitedVFNodes);
                        if (visited.find(item.getCurNodeID()) == visited.end()) {
                            visited.insert(item.getCurNodeID());
                            _spatialSlice.insert(vfNode->getICFGNode()->getId());
                            tmpLayer.push_back(SVFUtil::move(item));
                        }
                    }
                } // end branch data slicing
            }
        } // end control slicing
    } // end callsites control slicing
}

void SpatialSlicer::extCallSlicing(const CxtDPItem &curNode, std::vector<CxtDPItem> &tmpLayer,
                                   Set<u32_t> &visitedVFNodes, Set<u32_t> &visited) {
    PAG *pag = PAG::getPAG();
    if (const RetICFGNode *retICFGNode = SVFUtil::dyn_cast<RetICFGNode>(_curICFGNode->getICFGNode())) {
        if (_spatialSlice.find(retICFGNode->getCallICFGNode()->getId()) == _spatialSlice.end()) {
            _spatialSlice.insert(retICFGNode->getCallICFGNode()->getId());
        }
        if (PSAOptions::EnableExtCallSlicing()) {
            const CallICFGNode *callNode = retICFGNode->getCallICFGNode();
            bool isNotCall = SVFUtil::isExtCall(callNode->getCallSite());
            Set<const SVFFunction *> functionSet;
            getPTACallGraph()->getCallees(callNode, functionSet);
            if (!functionSet.empty()) {
                FSMParser::FSMAction action = getFSMParser()->getTypeFromStr((*functionSet.begin())->getName());
                if ((getFSMParser()->getFSMActions().count(action) && action != FSMParser::CK_DUMMY) ||
                    callNode->getRetICFGNode() == _curEvalICFGNode || callNode == _curEvalICFGNode ||
                    _curEvalFuns.count(*functionSet.begin()))
                    isNotCall = true;
            }
            if (isNotCall) {
                // handle external call
                NodeID lhsId = pag->getValueNode(callNode->getCallSite());
                CallSite cs(callNode->getCallSite());
                for (u32_t i = 0; i < cs.getNumArgOperands(); i++) {
                    const SVFGNode *vfNode = _svfg->getDefSVFGNode(
                            pag->getGNode(pag->getValueNode(cs.getArgOperand(i))));
                    NodeID nodeId = vfNode->getICFGNode()->getId();
                    if (!PSAOptions::EnableTemporalSlicing() ||
                        getICFGWrapper()->getICFGNodeWrapper(nodeId)->_inTSlice) {
                        CxtDPItem item(vfNode->getId(), curNode.getContexts());
                        extractGlobVars(vfNode, _globVars, visitedVFNodes);
                        if (visited.find(item.getCurNodeID()) == visited.end()) {
                            visited.insert(item.getCurNodeID());
                            _spatialSlice.insert(nodeId);
                            tmpLayer.push_back(SVFUtil::move(item));
                        }
                    }
                }
            } // end ext call
        }
    } // end return node

    if (const CallICFGNode *callNode = SVFUtil::dyn_cast<CallICFGNode>(_curICFGNode->getICFGNode())) {
        if (_spatialSlice.find(callNode->getId()) == _spatialSlice.end()) {
            _spatialSlice.insert(callNode->getId());
        }
        if (PSAOptions::EnableExtCallSlicing()) {
            bool isNotCall = SVFUtil::isExtCall(callNode->getCallSite());
            Set<const SVFFunction *> functionSet;
            getPTACallGraph()->getCallees(callNode, functionSet);
            if (!functionSet.empty()) {
                FSMParser::FSMAction action = getFSMParser()->getTypeFromStr((*functionSet.begin())->getName());
                if ((getFSMParser()->getFSMActions().count(action) && action != FSMParser::CK_DUMMY) ||
                    callNode->getRetICFGNode() == _curEvalICFGNode || callNode == _curEvalICFGNode ||
                    _curEvalFuns.count(*functionSet.begin()))
                    isNotCall = true;
            }
            if (isNotCall) {
                // handle external call
                NodeID lhsId = pag->getValueNode(callNode->getCallSite());
                CallSite cs(callNode->getCallSite());
                for (u32_t i = 0; i < cs.getNumArgOperands(); i++) {
                    const SVFGNode *vfNode = _svfg->getDefSVFGNode(
                            pag->getGNode(pag->getValueNode(cs.getArgOperand(i))));
                    NodeID nodeId = vfNode->getICFGNode()->getId();
                    if (!PSAOptions::EnableTemporalSlicing() ||
                        getICFGWrapper()->getICFGNodeWrapper(nodeId)->_inTSlice) {
                        CxtDPItem item(vfNode->getId(), curNode.getContexts());
                        extractGlobVars(vfNode, _globVars, visitedVFNodes);
                        if (visited.find(item.getCurNodeID()) == visited.end()) {
                            visited.insert(item.getCurNodeID());
                            _spatialSlice.insert(nodeId);
                            tmpLayer.push_back(SVFUtil::move(item));
                        }
                    }

                }
            } // end ext call
        }
    } // end call node
}

void SpatialSlicer::gepSlicing(const CxtDPItem &curNode, std::vector<CxtDPItem> &tmpLayer,
                               Set<u32_t> &visitedVFNodes, Set<u32_t> &visited) {
    PAG *pag = PAG::getPAG();
    ICFGNode::SVFStmtList svfStmtList = _curICFGNode->getICFGNode()->getSVFStmts();
    if (!svfStmtList.empty()) {
        const SVFStmt *svfStmt = *svfStmtList.begin();
        if (const GepStmt *gepStmt = dyn_cast<GepStmt>(svfStmt)) {
            for (int i = gepStmt->getOffsetVarAndGepTypePairVec().size() - 1; i >= 0; i--) {
                const SVFVar *offsetVar = gepStmt->getOffsetVarAndGepTypePairVec()[i].first;
                const SVFConstantInt *op = SVFUtil::dyn_cast<SVFConstantInt>(offsetVar->getValue());
                if (!op) {
                    /// variable as the offset
                    NodeID index = offsetVar->getId();
                    const SVFGNode *gepVFNode = _svfg->getDefSVFGNode(PAG::getPAG()->getGNode(index));
                    NodeID nodeId = gepVFNode->getICFGNode()->getId();
                    if (!PSAOptions::EnableTemporalSlicing() ||
                        getICFGWrapper()->getICFGNodeWrapper(nodeId)->_inTSlice) {
                        CxtDPItem item(gepVFNode->getId(), curNode.getContexts());
                        extractGlobVars(gepVFNode, _globVars, visitedVFNodes);
                        if (visited.find(item.getCurNodeID()) == visited.end()) {
                            visited.insert(item.getCurNodeID());
                            _spatialSlice.insert(nodeId);
                            tmpLayer.push_back(SVFUtil::move(item));
                        }
                    }

                }
            }
        } // end gep data slicing
    } // end special intra node
}

void SpatialSlicer::dataSlicing(const CxtDPItem &curNode, std::vector<CxtDPItem> &tmpLayer,
                                Set<u32_t> &visitedVFNodes, Set<CxtDPItem> &visitedCallGDPItems,
                                Set<u32_t> &visited) {
    PAG *pag = PAG::getPAG();
    extractGlobVars(_curSVFGNode, _globVars, visitedVFNodes);

    for (const auto &vEdge: _curSVFGNode->getInEdges()) {
        VFGNode *srcVFNode = vEdge->getSrcNode();
        const ICFGNode *srcCFNode = srcVFNode->getICFGNode();
        if (PSAOptions::EnableTemporalSlicing() &&
            !getICFGWrapper()->getICFGNodeWrapper(srcCFNode->getId())->_inTSlice)
            continue;
        CxtDPItem newItem(srcVFNode->getId(), curNode.getContexts());
        if (vEdge->isRetVFGEdge()) {
            FSMParser::FSMAction action = getFSMParser()->getTypeFromStr(
                    vEdge->getSrcNode()->getFun()->getName());
            if (_curEvalFuns.count(vEdge->getSrcNode()->getFun()) ||
                (getFSMParser()->getFSMActions().count(action) && action != FSMParser::CK_DUMMY))
                continue;
            CallSiteID csId = 0;
            if (const RetDirSVFGEdge *retEdge = SVFUtil::dyn_cast<RetDirSVFGEdge>(vEdge))
                csId = retEdge->getCallSiteId();
            else
                csId = SVFUtil::cast<RetIndSVFGEdge>(vEdge)->getCallSiteId();
            newItem.pushContext(csId);
        } else if (vEdge->isCallVFGEdge()) {
            FSMParser::FSMAction action = getFSMParser()->getTypeFromStr(
                    vEdge->getDstNode()->getFun()->getName());
            if (_curEvalFuns.count(vEdge->getDstNode()->getFun()) ||
                (getFSMParser()->getFSMActions().count(action) && action != FSMParser::CK_DUMMY))
                continue;
            CallSiteID csId = 0;
            if (const CallDirSVFGEdge *callEdge = SVFUtil::dyn_cast<CallDirSVFGEdge>(vEdge))
                csId = callEdge->getCallSiteId();
            else
                csId = SVFUtil::cast<CallIndSVFGEdge>(vEdge)->getCallSiteId();

            if (!newItem.matchContext(csId)) continue;
        }
        extractGlobVars(srcVFNode, _globVars, visitedVFNodes);
        /// whether this dstNode has been visited or not
        if (visited.count(newItem.getCurNodeID())) continue;
        else visited.insert(newItem.getCurNodeID());
        _spatialSlice.insert(srcCFNode->getId());
        if (const SVFFunction *fun = srcCFNode->getFun()) {
            NodeID callGraphId = getPTACallGraph()->getCallGraphNode(fun)->getId();
            CxtDPItem item(callGraphId, newItem.getContexts());
            if (!visitedCallGDPItems.count(item)) {
                visitedCallGDPItems.insert(item);
                _cGDpItems.insert(SVFUtil::move(item));
            }
        }
        tmpLayer.push_back(SVFUtil::move(newItem));
    } // end vfedges

    if (SVFUtil::isa<ActualOUTSVFGNode>(_curSVFGNode) || SVFUtil::isa<ActualRetVFGNode>(_curSVFGNode)) {
        for (const auto &actualin: getAbsTransitionHandler()->getReachableActualInsOfActualOut(_curSVFGNode)) {
            if (PSAOptions::EnableTemporalSlicing() &&
                !getICFGWrapper()->getICFGNodeWrapper(actualin->getICFGNode()->getId())->_inTSlice)
                continue;
            CxtDPItem newItem(actualin->getId(), curNode.getContexts());
            extractGlobVars(actualin, _globVars, visitedVFNodes);
            if (visited.count(newItem.getCurNodeID())) continue;
            else visited.insert(newItem.getCurNodeID());
            _spatialSlice.insert(actualin->getICFGNode()->getId());
            tmpLayer.push_back(SVFUtil::move(newItem));
        }
        if(const RetICFGNode* retNode = SVFUtil::dyn_cast<RetICFGNode>(_curICFGNode->getICFGNode())) {
            const CallICFGNode *callNode = retNode->getCallICFGNode();
            if (PSAOptions::EnableTemporalSlicing() &&
                !getICFGWrapper()->getICFGNodeWrapper(callNode->getId())->_inTSlice)
                return;
            if (const SVFFunction *fun = SVFUtil::getCallee(callNode->getCallSite())) {
                NodeID callGraphId = getPTACallGraph()->getCallGraphNode(fun)->getId();
                ContextCond curContext = curNode.getContexts();
                curContext.pushContext(getPTACallGraph()->getCallSiteID(callNode, fun));
                CxtDPItem item(callGraphId, curContext);
                if (!visitedCallGDPItems.count(item)) {
                    visitedCallGDPItems.insert(item);
                    _cGDpItems.insert(SVFUtil::move(item));
                }
            }
        }
    }
}

/*!
 * Extract the functions (callgraph nodes) of interest for each src
 * @param srcs
 */
void SpatialSlicer::callsitesExtraction(Set<CxtDPItem> &cGDpItems, Set<u32_t> &callSites, Set<CxtDPItem> &visited) {
    ICFG *icfg = PAG::getPAG()->getICFG();
    FIFOWorkList<CxtDPItem> workList;
    for (const auto &item: cGDpItems) {
        if (!visited.count(item)) {
            workList.push(item);
            visited.insert(item);
        }
    }
    while (!workList.empty()) {
        CxtDPItem curItem = workList.pop();
        for (const auto &e: getPTACallGraph()->getCallGraphNode(curItem.getCurNodeID())->getInEdges()) {
            CxtDPItem newItem(e->getSrcID(), curItem.getContexts());
            if (!newItem.matchContext(e->getCallSiteID())) continue;
            NodeID nodeId = getPTACallGraph()->getCallSite(e->getCallSiteID())->getId();
            if (!PSAOptions::EnableTemporalSlicing() ||
                getICFGWrapper()->getICFGNodeWrapper(nodeId)->_inTSlice) {
                callSites.insert(nodeId);
                if (visited.count(newItem)) continue;
                else visited.insert(newItem);
                workList.push(SVFUtil::move(newItem));
            }
        }
    }
}

