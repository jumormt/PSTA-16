//
// Created by Xiao on 2022/3/14.
//

#ifndef PSA_GRAPHSPARSIFICATOR_H
#define PSA_GRAPHSPARSIFICATOR_H

#include <utility>
#include "Slicing/PIExtractor.h"
#include "Slicing/ICFGWrapper.h"
#include <Util/DPItem.h>
#include "PSTA/FSMHandler.h"
#include "Slicing/TemporalSlicer.h"
#include "Slicing/SpatialSlicer.h"

namespace SVF {

class PSAStat;

/*!
 * Graph Sparsificator
 *
 * Graph slicing (temporal and spatial slicing) and graph compaction (delete nodes and edges)
 */
class GraphSparsificator {
    friend class PSAStat;

public:
    typedef PIExtractor::PI PI;
    typedef PIExtractor::SrcToPI SrcToPI;
    typedef FSMHandler::SrcSet SrcSet;
    typedef FSMHandler::ICFGNodeSet ICFGNodeSet;
    typedef PIExtractor::SrcToNodeIDSetMap SrcToNodeIDSetMap;
    typedef PIExtractor::NodeIDSet NodeIDSet;
    typedef PTACallGraph::FunctionSet FunctionSet;
    typedef Map<const SVFGNode *, Set<CxtDPItem>> SrcToCxtDPItemSetMap;
private:
    PSAStat *_stat{nullptr};
    NodeIDSet _temporalSlice;       ///< map source object (SVFGNode) to its temporal slice
    NodeIDSet _callsites;       ///< map source object (SVFGNode) to its passing callsites
    NodeIDSet _spatialSlice;       ///< map source object (SVFGNode) to its spatio slice
    Set<CxtDPItem> _cGDpItems;
    NodeIDSet _branch;    ///< map source object (SVFGNode) to the branches needed for analysis
    PI _PI;                  ///< map source object (SVFGNode) to its **reversed** operation sequences (e.g. use->free->malloc),
    ///< return node is used for API operation
    NodeIDSet _globVars;
    TemporalSlicer _temporalSlicer;
    SpatialSlicer _spatialSlicer;

    SrcToNodeIDSetMap _srcToTemporalSlice;       ///< map source object (SVFGNode) to its temporal slice
    SrcToNodeIDSetMap _srcToCallsites;       ///< map source object (SVFGNode) to its passing callsites
    SrcToNodeIDSetMap _srcToSpatialSlice;       ///< map source object (SVFGNode) to its spatio slice
    SrcToCxtDPItemSetMap _srcToCGDpItems;
    SrcToNodeIDSetMap _srcToBranch;    ///< map source object (SVFGNode) to the branches needed for analysis
    SrcToPI _srcToPI;                  ///< map source object (SVFGNode) to its operation sequences
    SrcToNodeIDSetMap _srcToGlobVars;

public:
    explicit GraphSparsificator();

    ~GraphSparsificator() = default;

    inline void setStat(PSAStat *stat) {
        _stat = stat;
    }

    static inline const std::unique_ptr<FSMParser> &getFSMParser() {
        return FSMParser::getFSMParser();
    }

    static inline const std::unique_ptr<ICFGWrapper> &getICFGWrapper() {
        return ICFGWrapper::getICFGWrapper();
    }


    inline void clearItems() {
        _PI.clear();
        _callsites.clear();
        _temporalSlice.clear();
        _spatialSlice.clear();
        _cGDpItems.clear();
        _branch.clear();
        _globVars.clear();
    }

    inline Set<u32_t> &getGlobalVars() {
        return _globVars;
    }

    inline PI &getSQ() {
        return _PI;
    }

    static inline const std::unique_ptr<FSMHandler> &getAbsTransitionHandler() {
        return FSMHandler::getAbsTransitionHandler();
    }

    static PTACallGraph *getPTACallGraph();


    /// Multi-point tempo-spatial slicing
    //{%
    /// Given checker types, extract all the operation sequences for each src
    inline void extractPI(SVFModule *svfModule, const SVFGNode *src, Set<FSMParser::CHECKER_TYPE> &checkerTypes,
                          const ICFGNodeSet &snks) {
        PIExtractor piExtractor;
        piExtractor.extract(svfModule, src, _PI, checkerTypes, snks);
    }

    virtual void
    multipointSlicing(SVFModule *svfModule, Set<FSMParser::CHECKER_TYPE> &checkerTypes, const SVFGNode *src,
                      ICFGNodeSet &snks, const ICFGNode *mainEntry);

    /// Spatial slicing
    inline void spatialSlicing(const SVFGNode *src, ICFGNodeSet &snks) {
        _spatialSlicer.spatialSlicing(src, snks);
    }

    /// Temporal slicing
    inline void temporalSlicing(const SVFGNode *src, const ICFGNodeWrapper *mainEntry) {
        _temporalSlicer.temporalSlicing(src, mainEntry);
    }

    /// Init snkMap for sparsification
    void initSnkMapViaSQ(const SVFGNode *src, ICFGNodeSet &snks);
    //%}

    /// Normal (spatial) slicing
    //{%
    virtual void normalSlicing(const SVFGNode *src, ICFGNodeSet &snks);
    //%}


    /// Judge node type
    //{%
    static inline const CallICFGNode *
    isCallNode(const ICFGNodeWrapper *nodeWrapper, const ICFGNode *curEvalICFGNode,
               Set<const SVFFunction *> &curEvalFuns) {
        return TemporalSlicer::isCallNode(nodeWrapper, curEvalICFGNode, curEvalFuns);
    }

    static inline const RetICFGNode *
    isRetNode(const ICFGNodeWrapper *nodeWrapper, const ICFGNode *curEvalICFGNode,
              Set<const SVFFunction *> &curEvalFuns) {
        return TemporalSlicer::isRetNode(nodeWrapper, curEvalICFGNode, curEvalFuns);
    }

    static inline const FunExitICFGNode *isExitNode(const ICFGNodeWrapper *nodeWrapper) {
        return TemporalSlicer::isExitNode(nodeWrapper);
    }

    static inline const FunEntryICFGNode *isEntryNode(const ICFGNodeWrapper *nodeWrapper) {
        return TemporalSlicer::isEntryNode(nodeWrapper);
    }
    //%}

    /// Graph Compaction
    //%{
    /// Compact graph for preprocessing (multi-point slicing)
    void compactGraph(const SVFGNode *curEvalSVFGNode, const ICFGNode *curEvalICFGNode,
                      Set<const SVFFunction *> &curEvalFuns, const ICFGNode *mainEntry,
                      OrderedSet<const ICFGNode *> &snks);


    /// Maps a call node to the name of the called function
    static inline void callee(const CallICFGNode *callBlockNode, FunctionSet &funSet) {
        for (const auto &edge: callBlockNode->getOutEdges()) {
            if (edge->isCallCFGEdge())
                funSet.insert(edge->getDstNode()->getFun());
        }
    }

    static inline void collectCallNodeToRemove(const CallICFGNode *callBlockNode, Set<NodeID> &N_c,
                                               Set<ICFGNodeWrapper *> &callNodesToRemove,
                                               ICFGNodeWrapper *nodeWrapper) {
        bool toRemove = true;
        FunctionSet funSet;
        callee(callBlockNode, funSet);
        for (const auto &fun: funSet) {
            if (N_c.find(getPTACallGraph()->getCallGraphNode(fun)->getId()) != N_c.end()) {
                toRemove = false;
                break;
            }
        }
        if (toRemove) callNodesToRemove.insert(nodeWrapper);
    }

    static inline void compactCallNodes(ICFGNodeWrapper *node) {
        Map<ICFGNodeWrapper *, ICFGEdge *> srcNodesToEdge;
        std::vector<ICFGNodeWrapper *> dstNodes;
        for (const auto &e: node->getInEdges()) {
            srcNodesToEdge[e->getSrcNode()] = e->getICFGEdge();
        }
        for (const auto &e: node->getRetICFGNodeWrapper()->getOutEdges()) {
            dstNodes.push_back(e->getDstNode());
        }
        for (const auto &srcNodeK: srcNodesToEdge) {
            for (const auto &dstNode: dstNodes) {
                if (!getICFGWrapper()->hasICFGEdgeWrapper(srcNodeK.first, dstNode)) {
                    ICFGEdgeWrapper *pEdge = new ICFGEdgeWrapper(srcNodeK.first, dstNode, srcNodeK.second);
                    getICFGWrapper()->addICFGEdgeWrapper(pEdge);
                } else {
                    if (const IntraCFGEdge *intraEdge = SVFUtil::dyn_cast<IntraCFGEdge>(
                            srcNodeK.second)) {
                        if (intraEdge->getCondition() && !getICFGWrapper()->hasICFGEdgeWrapper(srcNodeK.first, dstNode,
                                                                                               srcNodeK.second)) {
                            ICFGEdgeWrapper *pEdge = new ICFGEdgeWrapper(srcNodeK.first, dstNode,
                                                                         srcNodeK.second);
                            getICFGWrapper()->addICFGEdgeWrapper(pEdge);
                        }
                    }
                }
            }
        }
        ICFGNodeWrapper *retNode = node->getRetICFGNodeWrapper();
        getICFGWrapper()->removeICFGNodeWrapper(node);
        getICFGWrapper()->removeICFGNodeWrapper(retNode);
    }

    static inline void compactIntraNodes(ICFGNodeWrapper *node) {
        Map<ICFGNodeWrapper *, ICFGEdgeWrapper *> srcNodesToEdge;
        for (const auto &e: node->getInEdges()) {
            srcNodesToEdge[e->getSrcNode()] = e;

        }
        Map<ICFGNodeWrapper *, ICFGEdgeWrapper *> dstNodesToEdge;
        for (const auto &e: node->getOutEdges()) {
            dstNodesToEdge[e->getDstNode()] = e;
        }
        for (const auto &srcNodeK: srcNodesToEdge) {
            for (const auto &dstNodeK: dstNodesToEdge) {
                if (!getICFGWrapper()->hasICFGEdgeWrapper(srcNodeK.first, dstNodeK.first)) {
                    ICFGEdgeWrapper *pEdge = new ICFGEdgeWrapper(srcNodeK.first, dstNodeK.first,
                                                                 srcNodeK.second->getICFGEdge());
                    getICFGWrapper()->addICFGEdgeWrapper(pEdge);
                } else {
                    if (const IntraCFGEdge *intraEdge = SVFUtil::dyn_cast<IntraCFGEdge>(
                            srcNodeK.second->getICFGEdge())) {
                        if (intraEdge->getCondition() &&
                            !getICFGWrapper()->hasICFGEdgeWrapper(srcNodeK.first, dstNodeK.first,
                                                                  srcNodeK.second->getICFGEdge())) {
                            ICFGEdgeWrapper *pEdge = new ICFGEdgeWrapper(srcNodeK.first, dstNodeK.first,
                                                                         srcNodeK.second->getICFGEdge());
                            getICFGWrapper()->addICFGEdgeWrapper(pEdge);
                        }
                    }
                }
            }
        }
        getICFGWrapper()->removeICFGNodeWrapper(node);
    }

    static inline const CallICFGNode *
    isFSMCallNode(const ICFGNodeWrapper *nodeWrapper, const ICFGNode *curEvalICFGNode,
                  Set<const SVFFunction *> &curEvalFuns) {
        if (!nodeWrapper) return nullptr;
        const ICFGNode *node = nodeWrapper->getICFGNode();
        const auto *callBlockNode = SVFUtil::dyn_cast<const CallICFGNode>(node);
        if (!callBlockNode)
            return nullptr;
        // Do not enter callee if current callblocknode is a FSM action
        Set<const SVFFunction *> functionSet;
        getPTACallGraph()->getCallees(callBlockNode, functionSet);
        if (functionSet.empty())
            return nullptr;
        FSMParser::FSMAction action = getFSMParser()->getTypeFromStr((*functionSet.begin())->getName());
        if ((getFSMParser()->getFSMActions().count(action) && action != FSMParser::CK_DUMMY) ||
            callBlockNode->getRetICFGNode() == curEvalICFGNode || callBlockNode == curEvalICFGNode ||
            curEvalFuns.count(*functionSet.begin()))
            return callBlockNode;
        else
            return nullptr;
    }

    static inline const CallICFGNode *isExtCall(const ICFGNodeWrapper *nodeWrapper) {
        if (!nodeWrapper) return nullptr;
        const ICFGNode *node = nodeWrapper->getICFGNode();
        const auto *callBlockNode = SVFUtil::dyn_cast<const CallICFGNode>(node);
        if (!callBlockNode)
            return nullptr;
        if (SVFUtil::isExtCall(callBlockNode->getCallSite())) {
            return callBlockNode;
        } else
            return nullptr;
    }
    //%}

    /// Remove the function body of FSM call node
    static void removeFSMNodeBody(const ICFGNode *curEvalICFGNode, Set<const SVFFunction *> &curEvalFuns);

};


}


#endif //PSA_GRAPHSPARSIFICATOR_H
