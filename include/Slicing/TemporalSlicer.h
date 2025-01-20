//
// Created by Xiao on 7/24/2022.
//

#ifndef PSA_TEMPORALSLICER_H
#define PSA_TEMPORALSLICER_H

#include "PSTA/FSMHandler.h"
#include "Slicing/ICFGWrapper.h"
#include "Slicing/PIExtractor.h"

namespace SVF {


/*!
 * Temporal slicing
 *
 * Extracting all the control and data dependencies in a given temporal order
 */
class TemporalSlicer {


public:
    typedef FSMHandler::SrcSet SrcSet;
    typedef PIExtractor::PI SQ;
    typedef PIState::DataFact DataFact;
    typedef PIExtractor::SrcToPI SrcToSQ;
    typedef PIExtractor::SrcToNodeIDSetMap SrcToNodeIDSetMap;
    typedef PIExtractor::NodeIDSet NodeIDSet;

private:
    SQ &_sQ;                  ///< map source object (SVFGNode) to its operation sequences
    NodeIDSet& _temporalSlice; ///< map source object (SVFGNode) to its temporal slice

public:
    TemporalSlicer(SQ &sQ, NodeIDSet &temporalSlice) : _sQ(sQ), _temporalSlice(temporalSlice) {}

    virtual ~TemporalSlicer() = default;

    static inline const std::unique_ptr<ICFGWrapper> &getICFGWrapper() {
        return ICFGWrapper::getICFGWrapper();
    }

    static inline const std::unique_ptr<FSMParser> &getFSMParser() {
        return FSMParser::getFSMParser();
    }


    /// Temporal slicing
    void
    temporalSlicing(const SVFGNode* src, const ICFGNodeWrapper *mainEntry);

    static inline void clearDF() {
        for (const auto &n: *getICFGWrapper()) {
            n.second->_tdReachableDataFacts.clear();
            n.second->_buReachableDataFacts.clear();
            for (const auto &e: n.second->getOutEdges()) {
                e->_tdDataFactTransferFunc.clear();
                e->_buDataFactTransferFunc.clear();
            }
        }
    }

    static void initBUDFTransferFunc(const ICFGNodeWrapper *mainEntry, std::vector<DataFact> &allDataFacts);

    static void initTDDFTransferFunc(const ICFGNodeWrapper *mainEntry, std::vector<DataFact> &allDataFacts);

    /// Top-Down IFDS solver for tailoring
    static void
    tdIFDSSolve(const ICFGNodeWrapper *mainEntry, const ICFGNode *curEvalICFGNode,
                std::vector<DataFact> &allDataFacts, Set<const SVFFunction *> &curEvalFuns);

    /// Bottom-Up IFDS solver for tailoring
    static void
    buIFDSSolve(const ICFGNodeWrapper *mainEntry, const ICFGNodeWrapper *snk, const ICFGNode *curEvalICFGNode,
                std::vector<DataFact> &allDataFacts, Set<const SVFFunction *> &curEvalFuns);

    static void connectTowardsMainExit(const ICFGNodeWrapper* snkExit, Set<TPSIFDSItem>& pathEdge);

    static inline void propagate(FIFOWorkList<TPSIFDSItem> &workList, Set<TPSIFDSItem> &pathEdge, TPSIFDSItem &item) {
        if (!pathEdge.count(item)) {
            workList.push(item);
            pathEdge.insert(SVFUtil::move(item));
        }
    }

    static inline PTACallGraph *getPTACallGraph() {
        return AndersenWaveDiff::createAndersenWaveDiff(PAG::getPAG())->getPTACallGraph();
    }

    /// Judge node type
    //{%
    static inline const CallICFGNode *isCallNode(const ICFGNodeWrapper *nodeWrapper, const ICFGNode *curEvalICFGNode,
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
            return nullptr;
        bool hasEntry = false;
        for (const auto &edge: callBlockNode->getOutEdges()) {
            if (const CallCFGEdge *callCfgEdge = SVFUtil::dyn_cast<CallCFGEdge>(edge)) {
                if (SVFUtil::isa<FunEntryICFGNode>(callCfgEdge->getDstNode())) {
                    hasEntry = true;
                    break;
                }
            }
        }
        if (!hasEntry) return nullptr;
        bool hasRet = false;
        for (const auto &inEdge: callBlockNode->getRetICFGNode()->getInEdges()) {
            if (inEdge->isRetCFGEdge()) {
                hasRet = true;
                break;
            }
        }
        if (!hasRet) return nullptr;
        return callBlockNode;
    }

    static inline const RetICFGNode *isRetNode(const ICFGNodeWrapper *nodeWrapper, const ICFGNode *curEvalICFGNode,
                                               Set<const SVFFunction *> &curEvalFuns) {
        const auto *retICFGNode = SVFUtil::dyn_cast<const RetICFGNode>(nodeWrapper->getICFGNode());
        if (!retICFGNode)
            return nullptr;
        // Do not enter callee if current callblocknode is a FSM action
        if (isCallNode(nodeWrapper->getCallICFGNodeWrapper(), curEvalICFGNode, curEvalFuns)) {
            return retICFGNode;
        } else {
            return nullptr;
        }
    }

    static inline const FunExitICFGNode *isExitNode(const ICFGNodeWrapper *nodeWrapper) {
        return SVFUtil::dyn_cast<const FunExitICFGNode>(nodeWrapper->getICFGNode());
    }

    static inline const FunEntryICFGNode *isEntryNode(const ICFGNodeWrapper *nodeWrapper) {
        return SVFUtil::dyn_cast<const FunEntryICFGNode>(nodeWrapper->getICFGNode());
    }
    //%}

    /// Maps an exit node to its return-site nodes
    static inline void
    returnSites(const ICFGNodeWrapper *funExitBlockNode, std::vector<const ICFGNodeWrapper *> &toRet) {
        for (const auto &edge: funExitBlockNode->getOutEdges()) {
            if (edge->getICFGEdge()->isRetCFGEdge()) {
                const RetICFGNode *retBlockNode = SVFUtil::dyn_cast<RetICFGNode>(edge->getDstNode()->getICFGNode());
                assert(retBlockNode && "not return site?");
                toRet.push_back(edge->getDstNode());
            }
        }
    }

    /// Maps an exit node to its return-site nodes
    static inline void
    callSites(const ICFGNodeWrapper *funEntryICFGNode, std::vector<const ICFGNodeWrapper *> &callers) {
        for (const auto &edge: funEntryICFGNode->getInEdges()) {
            if (edge->getICFGEdge()->isCallCFGEdge()) {
                const CallICFGNode *callICFGNode = SVFUtil::dyn_cast<CallICFGNode>(
                        edge->getSrcNode()->getICFGNode());
                assert(callICFGNode && "not call site?");
                callers.push_back(edge->getSrcNode());
            }
        }
    }


}; // end class TemporalSlicer
} // end namespace SVF

#endif //PSA_TEMPORALSLICER_H
