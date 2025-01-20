//
// Created by z5489727 on 2023/7/25.
//

#ifndef TYPESTATE_SNKEXTRACTOR_H
#define TYPESTATE_SNKEXTRACTOR_H

#include <WPA/Andersen.h>
#include "SVF-LLVM/LLVMUtil.h"
#include "Slicing/PIExtractor.h"

namespace SVF {
class SNKExtractor {
public:
    typedef FSMHandler::SrcSet SrcSet;
    typedef FSMHandler::ICFGNodeSet ICFGNodeSet;
    typedef Set<u32_t> NodeIDSet;
    typedef Map<const SVFGNode *, NodeIDSet> SrcToNodeIDSetMap;
    typedef std::pair<const SVFFunction *, TypeState> SummaryKey;
    typedef Set<TypeState> TypeStates;
    typedef PTACallGraph::FunctionSet FunctionSet;
    typedef Map<const SVFFunction *, Set<const SVFBasicBlock *>> FunToExitBBsMap;  ///< map a function to all its basic blocks calling program exit
    typedef Map<SummaryKey, TypeStates> SummaryMap;

    typedef FIFOWorkList<WLItem> WorkList;

    enum PC_TYPE {
        UNK_PC = 0,
        TRUE_PC,
        FALSE_PC
    };


private:
    const ICFGNode *_curEvalICFGNode{nullptr};
    const SVFGNode *_curEvalSVFGNode{nullptr};
    Set<const SVFFunction *> _curEvalFuns;
    ICFGNodeSet _snks;
    SummaryMap _summaryMap;
    const ICFGNodeWrapper *_mainEntry{nullptr};
    TypeStates _emptyPIStates;
    FunToExitBBsMap _funToExitBBsMap;  ///< map a function to all its basic blocks calling program exit


public:
    SNKExtractor() = default;

    ~SNKExtractor() = default;

    static PTACallGraph *getPTACallGraph() {
        return AndersenWaveDiff::createAndersenWaveDiff(PAG::getPAG())->getPTACallGraph();
    }

    void initialize(SVFModule *svfModule);

    static const std::unique_ptr<FSMParser> &getFSMParser() {
        return FSMParser::getFSMParser();
    }

    static const std::unique_ptr<FSMHandler> &getAbsTransitionHandler() {
        return FSMHandler::getAbsTransitionHandler();
    }

    static inline const std::unique_ptr<ICFGWrapper> &getICFGWrapper() {
        return ICFGWrapper::getICFGWrapper();
    }

    void
    extract(SVFModule *svfModule, const SVFGNode *src, ICFGNodeSet &snks);

    void collectSnks();

    void initMap();

    static void clearMap();

    void solve();

    //{% Flow functions
    /// Flow function for processing merge node
    /// Combine two dataflow facts into a single fact, using set union
    inline TypeState
    mergeFlowFun(const ICFGNodeWrapper *icfgNodeWrapper, const TypeState &absState,
                 const TypeState &idxAbsState) {
        TypeStates typeStatesTmp;
        for (const auto &edge: icfgNodeWrapper->getInEdges()) {
            if (edge->getICFGEdge()->isIntraCFGEdge()) {
                const TypeState ts = getInfo(edge, absState, idxAbsState);
                if (ts != TypeState::Unknown) {
                    return ts;
                }
            }
        }
        return TypeState::Unknown;
    }

    static inline const FSMHandler::ICFGAbsTransitionFunc &getICFGAbsTransferMap() {
        return FSMHandler::getAbsTransitionHandler()->getICFGAbsTransferMap();
    }


    /// AbsTransitionHandler singleton
    static inline const std::unique_ptr<FSMHandler> &getFSMHandler() {
        return FSMHandler::getAbsTransitionHandler();
    }

    /// Flow function for processing non-branch node
    /// Symbolic state transition when current node contains transition actions (e.g., malloc, free)
    inline const TypeState &nonBranchFlowFun(const ICFGNodeWrapper *icfgNode, const TypeState &typeState) {
        auto it = getICFGAbsTransferMap().find(icfgNode->getICFGNode());
        if (it != getICFGAbsTransferMap().end()) {
            // _curEvalICFGNode is callnode
            auto it2 = it->second.find(typeState);
            assert(it2 != it->second.end() && "unknown typestate!");
            return it2->second;
        } else {
            // no abstract state transition
            return typeState;
        }
    }
    //%}

    inline bool addInfo(const ICFGEdgeWrapper *e, const TypeState &srcState, const TypeState &dstState) {
        if (dstState == TypeState::Unknown) return false;
        auto *edge = const_cast<ICFGEdgeWrapper *>(e);
        auto it = edge->_snkInfoMap.find(srcState);
        if (it == edge->_snkInfoMap.end()) {
            edge->_snkInfoMap[srcState] = {dstState};
            return true;
        } else {
            auto absIt = it->second.find(dstState);
            if (absIt == it->second.end()) {
                it->second.insert(dstState);
                return true;
            } else {
                return false;
            }
        }
    }

    inline const TypeState
    getInfo(const ICFGEdgeWrapper *e, const TypeState &absState, const TypeState &idxAbsState) {
        auto it = e->_snkInfoMap.find(absState);
        if (it == e->_snkInfoMap.end()) {
            return TypeState::Unknown;
        } else {
            auto absIt = it->second.find(idxAbsState);
            if (absIt == it->second.end()) {
                return TypeState::Unknown;
            } else {
                return idxAbsState;
            }
        }
    }

    inline bool
    addTrigger(const ICFGNodeWrapper *node, const TypeState &typeState) {
        assert(!node->getOutEdges().empty() && "SNKExtractor::addTrigger");
        const ICFGEdgeWrapper *edge = *node->getOutEdges().begin();
        return addInfo(edge, typeState, typeState);
    }

    inline bool addToSummary(const ICFGNodeWrapper *node, const TypeState &srcState, const TypeState &dstState) {
        if (dstState == TypeState::Unknown) return false;
        SummaryKey summaryKey = std::make_pair(fn(node), srcState);
        auto it = _summaryMap.find(summaryKey);
        if (it != _summaryMap.end()) {
            auto absIdxIt = it->second.find(dstState);
            if (absIdxIt == it->second.end()) {
                it->second.insert(dstState);
                return true;
            } else {
                return false;
            }
        } else {
            _summaryMap[summaryKey] = {dstState};
        }
        return true;
    }

    inline const TypeState applySummary(const TypeState &curState, const TypeState &summary) {
        return summary;
    }

    inline TypeState
    getSummary(const SVFFunction *fun, const TypeState &absState, const TypeState &idxAbsState) {
        SummaryKey summaryKey = std::make_pair(fun, absState);
        auto it = _summaryMap.find(summaryKey);
        if (it == _summaryMap.end())
            return TypeState::Unknown;
        else {
            auto absIt = it->second.find(idxAbsState);
            if (absIt == it->second.end()) {
                return TypeState::Unknown;
            } else {
                return idxAbsState;
            }
        }
    }

    inline bool hasSummary(const SVFFunction *fun, const TypeState &absState) {
        for (const auto &as: getFSMParser()->getAbsStates()) {
            if (getSummary(fun, absState, as) != TypeState::Unknown)
                return true;
        }
        return false;
    }

    /// Maps a node to the name of its enclosing function
    static inline const SVFFunction *fn(const ICFGNodeWrapper *nodeWrapper) {
        return nodeWrapper->getICFGNode()->getFun();
    }

    /// Maps a function name to its entry node
    static inline const ICFGNodeWrapper *entryNode(const SVFFunction *svfFunction) {
        return getICFGWrapper()->getFunEntry(svfFunction);
    }

    /// Maps a call node to the name of the called function
    static inline void callee(const CallICFGNode *callBlockNode, FunctionSet &funSet) {
        for (const auto &edge: callBlockNode->getOutEdges()) {
            if (edge->isCallCFGEdge())
                funSet.insert(edge->getDstNode()->getFun());
        }
    }

    /// Maps a return-site node to its call-site node
    static inline const ICFGNodeWrapper *callSite(const ICFGNodeWrapper *retBlockNode) {
        return retBlockNode->getCallICFGNodeWrapper();
    }

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

    /// Judge node type
    //{%
    inline const CallICFGNode *isCallNode(const ICFGNodeWrapper *nodeWrapper) const {
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
            callBlockNode->getRetICFGNode() == _curEvalICFGNode || callBlockNode == _curEvalICFGNode ||
            _curEvalFuns.count(*functionSet.begin()))
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

    static inline const FunExitICFGNode *isExitNode(const ICFGNodeWrapper *node) {
        return SVFUtil::dyn_cast<const FunExitICFGNode>(node->getICFGNode());
    }

    static inline bool isBranchNode(const ICFGNodeWrapper *nodeWrapper) {
        u32_t ct = 0;
        for (const auto &edge: nodeWrapper->getICFGNode()->getOutEdges()) {
            if (const IntraCFGEdge *intraCfgEdge = SVFUtil::dyn_cast<IntraCFGEdge>(edge)) {
                if (intraCfgEdge->getCondition())
                    ct++;
            }
        }
        return ct > 1;
    }

    static inline bool isMergeNode(const ICFGNodeWrapper *node) {
        u32_t ct = 0;
        for (const auto &edge: node->getInEdges()) {
            if (edge->getICFGEdge()->isIntraCFGEdge())
                ct++;
        }
        return ct > 1;
    }
    //%}

    static inline Set<const ICFGEdge *> getInTEdges(const ICFGNode *node) {
        Set<const ICFGEdge *> edges;
        for (const auto &edge: node->getInEdges()) {
            if (edge->isIntraCFGEdge())
                edges.insert(edge);
        }
        return edges;
    }

    static inline Set<const ICFGEdge *> getOutTEdges(const ICFGNode *node) {
        Set<const ICFGEdge *> edges;
        for (const auto &edge: node->getOutEdges()) {
            if (edge->isIntraCFGEdge())
                edges.insert(edge);
        }
        return SVFUtil::move(edges);
    }

    static inline Set<const ICFGEdgeWrapper *> getInTEdges(const ICFGNodeWrapper *node) {
        Set<const ICFGEdgeWrapper *> edges;
        for (const auto &edge: node->getInEdges()) {
            if (edge->getICFGEdge()->isIntraCFGEdge())
                edges.insert(edge);
        }
        return SVFUtil::move(edges);
    }

    static inline Set<const ICFGEdgeWrapper *> getOutTEdges(const ICFGNodeWrapper *node) {
        Set<const ICFGEdgeWrapper *> edges;
        for (const auto &edge: node->getOutEdges()) {
            if (edge->getICFGEdge()->isIntraCFGEdge())
                edges.insert(edge);
        }
        return SVFUtil::move(edges);
    }

    static inline const ICFGNodeWrapper *nextNodeToAdd(const ICFGNodeWrapper *icfgNodeWrapper) {
        if (const CallICFGNode *callICFGNode = SVFUtil::dyn_cast<CallICFGNode>(icfgNodeWrapper->getICFGNode()))
            return icfgNodeWrapper->getRetICFGNodeWrapper();
        else return icfgNodeWrapper;
    }

    //{% Some heuristics
    /*!
     * Return true if:
     * (1) cmp contains a null value
     * (2) there is an indirect edge from cur evaluated SVFG node to cmp operand
     *
     * e.g.,
     *      cur svfg node -> 1. store i32* %0, i32** %p, align 8, !dbg !157
     *      cmp operand   -> 2. %1 = load i32*, i32** %p, align 8, !dbg !159
     *                       3. %tobool = icmp ne i32* %1, null, !dbg !159
     *                       4. br i1 %tobool, label %if.end, label %if.then, !dbg !161
     *     There is an indirect edge 1->2 with value %0
     */
    bool isTestContainsNullAndTheValue(const CmpStmt *cmp);

    static inline bool isEQCmp(const CmpStmt *cmp) {
        return (cmp->getPredicate() == CmpInst::ICMP_EQ);
    }

    static inline bool isNECmp(const CmpStmt *cmp) {
        return (cmp->getPredicate() == CmpInst::ICMP_NE);
    }

    inline bool isTestNullExpr(const SVFValue *test) {
        if (const SVFInstruction *svfInst = SVFUtil::dyn_cast<SVFInstruction>(test)) {
            for (const SVFStmt *stmt: PAG::getPAG()->getSVFStmtList(PAG::getPAG()->getICFG()->getICFGNode(svfInst))) {
                if (const CmpStmt *cmp = SVFUtil::dyn_cast<CmpStmt>(stmt)) {
                    return isTestContainsNullAndTheValue(cmp) && isEQCmp(cmp);
                }
            }
        }
        return false;
    }

    inline bool isTestNotNullExpr(const SVFValue *test) {
        if (const SVFInstruction *svfInst = SVFUtil::dyn_cast<SVFInstruction>(test)) {
            for (const SVFStmt *stmt: PAG::getPAG()->getSVFStmtList(PAG::getPAG()->getICFG()->getICFGNode(svfInst))) {
                if (const CmpStmt *cmp = SVFUtil::dyn_cast<CmpStmt>(stmt)) {
                    return isTestContainsNullAndTheValue(cmp) && isNECmp(cmp);
                }
            }
        }
        return false;
    }

    /// Evaluate null like expression for source-sink related bug detection in SABER
    PC_TYPE evaluateTestNullLikeExpr(const BranchStmt *cmpInst, const IntraCFGEdge *edge);

    /// Return condition when there is a branch calls program exit
    PC_TYPE evaluateProgExit(const BranchStmt *brInst, const SVFBasicBlock *succ);

    PC_TYPE evalBranchCond(const IntraCFGEdge *intraCfgEdge);

    inline LLVMModuleSet *getLLVMModuleSet() {
        return LLVMModuleSet::getLLVMModuleSet();
    }

    /// Collect basic block contains program exit function call
    void collectBBCallingProgExit(const SVFBasicBlock &bb);

    bool isBBCallsProgExit(const SVFBasicBlock *bb);

    inline bool postDominate(const SVFBasicBlock *bbKey, const SVFBasicBlock *bbValue) const {
        const SVFFunction *keyFunc = bbKey->getParent();
        const SVFFunction *valueFunc = bbValue->getParent();
        bool funcEq = (keyFunc == valueFunc);
        (void) funcEq; // Suppress warning of unused variable under release build
        assert(funcEq && "two basicblocks should be in the same function!");
        return keyFunc->postDominate(bbKey, bbValue);
    }
    //%}
};
}


#endif //TYPESTATE_SNKEXTRACTOR_H
