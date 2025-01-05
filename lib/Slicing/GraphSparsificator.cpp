//
// Created by Xiao on 2022/3/14.
//

#include "Slicing/GraphSparsificator.h"
#include "SABER/SaberSVFGBuilder.h"
#include "PSTA/PSAOptions.h"
#include "Slicing/ControlDGBuilder.h"
#include "PSTA/PSAStat.h"
#include "Util/SVFUtil.h"
#include "PSTA/Logger.h"

using namespace SVF;
using namespace SVFUtil;
using namespace std;


GraphSparsificator::GraphSparsificator()
        : _temporalSlicer(_PI, _temporalSlice),
          _spatialSlicer(_temporalSlice, _callsites, _spatialSlice, _cGDpItems, _branch,
                         _PI, _globVars) {
}

PTACallGraph *GraphSparsificator::getPTACallGraph() {
    SVFIR *svfir = PAG::getPAG();
    AndersenWaveDiff *ander = AndersenWaveDiff::createAndersenWaveDiff(svfir);
    return ander->getPTACallGraph();
}

/*!
 * Init the multi-point tempo-spatial slice
 * @param checkerTypes sink point types (the operation causing error state)
 */
void
GraphSparsificator::multipointSlicing(SVFModule *svfModule, Set<FSMParser::CHECKER_TYPE> &checkerTypes,
                                      const SVFGNode *src,
                                      ICFGNodeSet &snks, const ICFGNode *mainEntry) {
    // extract operation sequences for each source
    _stat->seqsStart();
    extractPI(svfModule, src, checkerTypes, snks);
    _stat->seqsEnd();
    _srcToPI[src] = _PI;

    // rebuild icfgwrapper after extracting PI
    ICFGWrapperBuilder builder;
    ICFG *icfg = PAG::getPAG()->getICFG();
    builder.build(icfg);

    // extract multi-point temporal slice
    _stat->ntStart();
    u32_t icfgNodeNum = PAG::getPAG()->getICFG()->getTotalNodeNum();
    if (PSAOptions::EnableTemporalSlicing()) {
        Log(LogLevel::Info) << "Before temporal slicing size: " << std::to_string(icfgNodeNum) << "\n";
        Dump() << "Before temporal slicing size: " << std::to_string(icfgNodeNum) << "\n";
        temporalSlicing(src, getICFGWrapper()->getICFGNodeWrapper(mainEntry->getId()));
        Log(LogLevel::Info) << "After temporal slicing size: " << std::to_string(_temporalSlice.size()) << "\n";
        Dump() << "After temporal slicing size: " << std::to_string(_temporalSlice.size()) << "\n";
    }
    _stat->ntEnd();
    _srcToTemporalSlice[src] = _temporalSlice;
    // extract sinks based on operation sequences
    initSnkMapViaSQ(src, snks);
    // extract multi-point spatial slice
    _stat->nsStart();
    if (PSAOptions::EnableSpatialSlicing()) {
        Log(LogLevel::Info) << "Before spatial slicing size: " << std::to_string(icfgNodeNum) << "\n";
        Dump() << "Before spatial slicing size: " << std::to_string(icfgNodeNum) << "\n";
        spatialSlicing(src, snks);
        Log(LogLevel::Info) << "After spatial slicing size: " << std::to_string(_spatialSlice.size()) << "\n";
        Dump() << "After spatial slicing size: " << std::to_string(_spatialSlice.size()) << "\n";
    }

    _stat->nsEnd();
    _srcToSpatialSlice[src] = _spatialSlice;
    _srcToCallsites[src] = _callsites;
}

void GraphSparsificator::normalSlicing(const SVFGNode *src, ICFGNodeSet &snks) {
    _stat->trackingBranchStart();
    if (PSAOptions::EnableSpatialSlicing())
        spatialSlicing(src, snks);
    _srcToSpatialSlice[src] = _spatialSlice;
    _srcToBranch[src] = _branch;
    _stat->trackingBranchEnd();
    _srcToCallsites[src] = _callsites;
}

/*!
 * Init snkMap for sparsification
 * @param srcToSQ
 * @param srcs
 * @param snkMap
 */
void GraphSparsificator::initSnkMapViaSQ(const SVFGNode *src, ICFGNodeSet &snks) {
    Log(LogLevel::Info) << "Init snk points via PI...";
    Dump() << "Init snk points via PI...";
    ICFG *icfg = PAG::getPAG()->getICFG();
    OrderedSet<const ICFGNode *> tmpsnks;
    for (const auto &dataFact: _PI) {
        ICFGNode *node = icfg->getICFGNode(dataFact[0]);
        if (const RetICFGNode *retNode = dyn_cast<RetICFGNode>(node)) {
            tmpsnks.insert(retNode->getCallICFGNode());
        } else {
            tmpsnks.insert(node);
        }
    }
    snks = SVFUtil::move(tmpsnks);
    Log(LogLevel::Info) << SVFUtil::sucMsg("[done]\n");
    Dump() << SVFUtil::sucMsg("[done]\n");
}

/*!
 * Compact graph for preprocessing (multi-point slicing)
 * @param curEvalSVFGNode
 * @param curEvalICFGNode
 * @param mainEntry
 */
void
GraphSparsificator::compactGraph(const SVFGNode *curEvalSVFGNode, const ICFGNode *curEvalICFGNode,
                                 Set<const SVFFunction *> &curEvalFuns,
                                 const ICFGNode *mainEntry, OrderedSet<const ICFGNode *> &snks) {
    Log(LogLevel::Info) << "Compacting graph...";
    Dump() << "Compacting graph...";
    ICFG *icfg = PAG::getPAG()->getICFG();
    if (PSAOptions::DumpICFGWrapper()) {
        Set<u32_t> fsmNodes;
        if (!PSAOptions::MultiSlicing()) {
            for (const auto &item: getAbsTransitionHandler()->getICFGAbsTransferMap()) {
                fsmNodes.insert(item.first->getId());
            }
        } else {
            for (const auto &sq: _PI) {
                for (const auto &id: sq) {
                    fsmNodes.insert(id);
                }
            }
        }
        getICFGWrapper()->annotateFSMNodes(fsmNodes);
    }

    if (PSAOptions::EnableTemporalSlicing()) {
        getICFGWrapper()->annotateTemporalSlice(_temporalSlice);
    }

    if (PSAOptions::EnableSpatialSlicing()) {
        getICFGWrapper()->annotateSpatialSlice(_spatialSlice);
        getICFGWrapper()->annotateCallsites(_callsites);
    }
    getICFGWrapper()->getICFGNodeWrapper(mainEntry->getId())->_inTSlice = true;
    getICFGWrapper()->getICFGNodeWrapper(mainEntry->getId())->_inSSlice = true;
    getICFGWrapper()->getICFGNodeWrapper(icfg->getFunExitICFGNode(mainEntry->getFun())->getId())->_inTSlice = true;
    getICFGWrapper()->getICFGNodeWrapper(icfg->getFunExitICFGNode(mainEntry->getFun())->getId())->_inSSlice = true;
    if (PSAOptions::MultiSlicing() && PSAOptions::EnableTemporalSlicing()) {
        for (const auto &snk: snks) {
            u32_t nodeId = snk->getId();
            if (const CallICFGNode *callNode = dyn_cast<CallICFGNode>(snk)) {
                nodeId = callNode->getRetICFGNode()->getId();
            }
            for (const auto &e: getICFGWrapper()->getICFGNodeWrapper(nodeId)->getOutEdges()) {
                if (e->getDstNode()->getRetICFGNodeWrapper() && !e->getDstNode()->_validCS) {
                    e->getDstNode()->setRetICFGNodeWrapper(nullptr);
                    e->getDstNode()->_validCS = true;
                }
                e->getDstNode()->_inSSlice = true;
            }
        }
    }

    if (PSAOptions::DumpICFGWrapper())
        getICFGWrapper()->dump("CFGO_" + std::to_string(curEvalICFGNode->getId()));

    // Remove irrelevant ICFG nodes
    Set<ICFGNodeWrapper *> callNodesToCompact, intraNodesToCompact, nodesToRemove;
    for (const auto &item: *getICFGWrapper()) {
        if (PSAOptions::EnableSpatialSlicing()) {
            // Spatial slicing is enabled
            // Remove callsites if spatial slicing is enabled
            if (isCallNode(item.second, curEvalICFGNode, curEvalFuns)) {
                if (PSAOptions::MultiSlicing() && PSAOptions::EnableTemporalSlicing()) {
                    if (!item.second->_inTSlice) {
                        nodesToRemove.insert(item.second);
                        nodesToRemove.insert(item.second->getRetICFGNodeWrapper());
                    } else if (!item.second->_validCS)
                        callNodesToCompact.insert(item.second);
                } else {
                    if (!item.second->_validCS) callNodesToCompact.insert(item.second);
                }
//                collectCallNodeToRemove(callBlockNode, _callsites, callNodesToRemove, item.second);
            } else if (isCallNode(item.second->getCallICFGNodeWrapper(), curEvalICFGNode, curEvalFuns)) {
                // Skip return node
                continue;
            } else if (isFSMCallNode(item.second, curEvalICFGNode, curEvalFuns)) {
                if (PSAOptions::MultiSlicing()) {
                    if (PSAOptions::EnableTemporalSlicing()) {
                        // Temporal and Spatial Slicing enabled
                        if (!item.second->_inTSlice) {
                            nodesToRemove.insert(item.second);
                            nodesToRemove.insert(item.second->getRetICFGNodeWrapper());
                        } else if (!item.second->_inSSlice) {
                            callNodesToCompact.insert(item.second);
                        }
                    } else {
                        // Only Spatial Slicing enabled
                        if (!item.second->_inSSlice)
                            callNodesToCompact.insert(item.second);
                    }
                } else {
                    if (!item.second->_inSSlice)
                        callNodesToCompact.insert(item.second);
                }
            } else if (isFSMCallNode(item.second->getCallICFGNodeWrapper(), curEvalICFGNode, curEvalFuns)) {
                continue;
            } else if (isExtCall(item.second)) {
                if (PSAOptions::MultiSlicing()) {
                    if (PSAOptions::EnableTemporalSlicing()) {
                        // when the outgoing neighbour of a sink is a call node, we set the retnode as null
                        // in this case, we keep the node
                        if(!item.second->getRetICFGNodeWrapper()) continue;
                        // Temporal and Spatial Slicing enabled
                        if (!item.second->_inTSlice && !item.second->getRetICFGNodeWrapper()->_inTSlice) {
                            nodesToRemove.insert(item.second);
                            nodesToRemove.insert(item.second->getRetICFGNodeWrapper());
                        } else if (!item.second->_inSSlice && !item.second->getRetICFGNodeWrapper()->_inSSlice) {
                            callNodesToCompact.insert(item.second);
                        }
                    } else {
                        // Only Spatial Slicing enabled
                        if (!item.second->_inSSlice && !item.second->getRetICFGNodeWrapper()->_inSSlice)
                            callNodesToCompact.insert(item.second);
                    }
                } else {
                    if (!item.second->_inSSlice && !item.second->getRetICFGNodeWrapper()->_inSSlice)
                        callNodesToCompact.insert(item.second);
                }
            } else if (isExtCall(item.second->getCallICFGNodeWrapper())) {
                continue;
            } else {
                // Non-callsites node
                if (PSAOptions::MultiSlicing()) {
                    if (PSAOptions::EnableTemporalSlicing()) {
                        // Temporal and Spatial Slicing enabled
                        if (!item.second->_inTSlice) {
                            nodesToRemove.insert(item.second);
                        } else if (!item.second->_inSSlice) {
                            intraNodesToCompact.insert(item.second);
                        }
                    } else {
                        // Only Spatial Slicing enabled
                        if (!item.second->_inSSlice)
                            intraNodesToCompact.insert(item.second);
                    }
                } else {
                    if (!item.second->_inSSlice)
                        intraNodesToCompact.insert(item.second);
                }
            }
        } else {
            // Spatial slicing is disabled
            if (PSAOptions::MultiSlicing()) {
                if (PSAOptions::EnableTemporalSlicing()) {
                    // Only temporal Slicing enabled
                    // Remove callsites
                    if (isCallNode(item.second, curEvalICFGNode, curEvalFuns)) {
                        if (!item.second->_inTSlice) {
                            nodesToRemove.insert(item.second);
                            nodesToRemove.insert(item.second->getRetICFGNodeWrapper());
                        }
                    } else if (isCallNode(item.second->getCallICFGNodeWrapper(), curEvalICFGNode, curEvalFuns)) {
                        // Skip return node
                        continue;
                    } else if (isFSMCallNode(item.second, curEvalICFGNode, curEvalFuns)) {
                        if (!item.second->_inTSlice) {
                            nodesToRemove.insert(item.second);
                            nodesToRemove.insert(item.second->getRetICFGNodeWrapper());
                        }
                    } else if (isFSMCallNode(item.second->getCallICFGNodeWrapper(), curEvalICFGNode, curEvalFuns)) {
                        continue;
                    } else if (isExtCall(item.second)) {
                        if (!item.second->_inTSlice && !item.second->getRetICFGNodeWrapper()->_inTSlice) {
                            nodesToRemove.insert(item.second);
                            nodesToRemove.insert(item.second->getRetICFGNodeWrapper());
                        }
                    } else if (isExtCall(item.second->getCallICFGNodeWrapper())) {
                        continue;
                    } else {
                        if (!item.second->_inTSlice)
                            nodesToRemove.insert(item.second);
                    }
                } else {
                    // The graph is unchanged if both temporal and spatial slicing are disabled
                }
            } else {
                // The graph is unchanged if multi-point slicing and spatial slicing are disabled
            }
        }
    }
    removeFSMNodeBody(curEvalICFGNode, curEvalFuns);
    for (const auto &node: nodesToRemove) {
        if (isa<GlobalICFGNode>(node->getICFGNode())) continue;
        getICFGWrapper()->removeICFGNodeWrapper(node);
    }
    for (const auto &node: callNodesToCompact) {
        compactCallNodes(node);
    }

    for (const auto &node: intraNodesToCompact) {
        compactIntraNodes(node);
    }


    if (PSAOptions::DumpICFGWrapper()) {
        getICFGWrapper()->removeFilledColor();
        getICFGWrapper()->dump("CFG_" + std::to_string(curEvalICFGNode->getId()));
    }
    Log(LogLevel::Info) << SVFUtil::sucMsg("[done]\n");
    Dump() << SVFUtil::sucMsg("[done]\n");
}

/*!
 * Remove the function body of FSM call node
 * @param curEvalICFGNode
 * @param curEvalFuns
 */
void GraphSparsificator::removeFSMNodeBody(const ICFGNode *curEvalICFGNode, Set<const SVFFunction *> &curEvalFuns) {
    Set<ICFGNodeWrapper *> toRemove;
    for (const auto &it: *getICFGWrapper()) {
        if (isFSMCallNode(it.second, curEvalICFGNode, curEvalFuns)) {
            toRemove.insert(it.second);
        }
    }
    for (const auto &it: toRemove) {
        ICFGNodeWrapper *retNode = it->getRetICFGNodeWrapper();
        Set<ICFGEdgeWrapper *> edgeToRm;
        for (const auto &e: it->getOutEdges()) {
            edgeToRm.insert(e);
        }
        for (const auto &e: retNode->getInEdges()) {
            edgeToRm.insert(e);
        }
        for (auto &e: edgeToRm) {
            getICFGWrapper()->removeICFGEdgeWrapper(e);
        }
        it->addOutgoingEdge(new ICFGEdgeWrapper(it, retNode, nullptr));
    }
}