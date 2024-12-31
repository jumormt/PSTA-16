//
// Created by Xiao on 7/26/2022.
//

#ifndef PSA_PSTABASE_H
#define PSA_PSTABASE_H

#include "Slicing/GraphSparsificator.h"
#include "PSTA/PSTABase.h"
#include "PSTA/BTPExtractor.h"


namespace SVF {
class PSAStat;



/*!
 * Path-sensitive typestate analysis
 */
class PSTA: public PSTABase {

    friend class PSAStat;

public:
    typedef std::pair<const ICFGEdgeWrapper *, TypeState> InfoKey; ///< a pair of ICFG edge and abstract state
    typedef Map<InfoKey, AbsToSymState> InfoMap;
    typedef FIFOWorkList<WLItem> WorkList;                                   ///< worklist for ESP-ICFG
    typedef PIExtractor::SrcToNodeIDSetMap SrcToNodeIDSetMap;


protected:
    GraphSparsificator _graphSparsificator;
    InfoMap _infoMap;
    PSAStat *_stat;
    WorkList _workList;
    SrcToNodeIDSetMap _srcToESPBranch;
public:

    /// Constructor
    PSTA();

    /// Destructor
    virtual ~PSTA();

    /// We start from here
    virtual bool runFSMOnModule(SVFModule *module) = 0;

    /// Analyzing entry
    virtual void analyze(SVFModule *module);

    /// Initialization
    virtual void initialize(SVFModule *module);

    virtual void initHandler(SVFModule *module);

    /// Main algorithm
    virtual void solve();

    /// Process node
    //{%
    /// Process Call Node
    virtual void processCallNode(const CallICFGNode *callBlockNode, WLItem &wlItem);

    virtual void processCallNodeIso(const CallICFGNode *callBlockNode, WLItem &wlItem);

    /// Process Exit Node
    virtual void processExitNode(WLItem &wlItem);

    virtual void processExitNodeIso(WLItem &wlItem);

    /// Process Branch Node
    virtual void processBranchNode(WLItem &wlItem);

    /// Process Other Node
    virtual void processOtherNode(WLItem &wlItem);
    //%}

    virtual inline bool addTrigger(const ICFGEdgeWrapper *e, SymState symState) {
        TypeState curAbsState = symState.getAbstractState();
        return addInfo(e, curAbsState, SVFUtil::move(symState));
    }

    virtual inline bool
    addToSummary(const ICFGNodeWrapper *nodeWrapper, const TypeState &absState, SymState &symState) {
        if (symState.isNullSymState()) return false;
        SummaryKey summaryKey = std::make_pair(fn(nodeWrapper), absState);
        auto it = _summaryMap.find(summaryKey);
        if (it != _summaryMap.end()) {
            auto absIdxIt = it->second.find(symState.getAbstractState());
            if (absIdxIt == it->second.end()) {
                it->second[symState.getAbstractState()] = SVFUtil::move(symState);
                return true;
            } else {
                SymStates symStates{absIdxIt->second}; // original symstate
                symStates.push_back(SVFUtil::move(symState));
                SymState symStateOut;
                if (groupingAbsStates(symStates, symStateOut)) { // symstate changed
                    absIdxIt->second = SVFUtil::move(symStateOut);
                    return true;
                } else
                    return false;
            }
        } else {
            _summaryMap[summaryKey][symState.getAbstractState()] = SVFUtil::move(symState);
        }
        return true;
    }

    virtual inline bool
    addTriggerIso(const ICFGEdgeWrapper *edge, SymState symState) {
        TypeState absState = symState.getAbstractState();
        return addInfo(edge, absState, SVFUtil::move(symState));
    }

    virtual inline bool
    addToSummaryIso(const ICFGNodeWrapper *nodeWrapper, const TypeState &absState, SymState symState) {
        if (symState.isNullSymState()) return false;
        const FunExitICFGNode *funExitBlockNode = SVFUtil::dyn_cast<const FunExitICFGNode>(
                nodeWrapper->getICFGNode());
        SummaryKey summaryKey = std::make_pair(fn(nodeWrapper), absState);
        Set<const FormalOUTSVFGNode *> formalOuts = getFormalOutSVFGNodes(funExitBlockNode);
        SymState toSummary = SVFUtil::move(
                extractToSummarySymState(symState, funExitBlockNode->getFormalRet(), formalOuts));
        auto it = _summaryMap.find(summaryKey);
        if (it == _summaryMap.end()) {
            _summaryMap[summaryKey][toSummary.getAbstractState()] = SVFUtil::move(toSummary);
            return true;
        } else {
            auto absIdxIt = it->second.find(toSummary.getAbstractState());
            if (absIdxIt == it->second.end()) {
                it->second[toSummary.getAbstractState()] = SVFUtil::move(toSummary);
                return true;
            } else {
                SymStates symStates{absIdxIt->second}; // original symstate
                symStates.push_back(SVFUtil::move(toSummary));
                SymState symStateOut;
                if (!groupingAbsStates(symStates, symStateOut)) {
                    return false;
                } else { // symstate changed
                    absIdxIt->second = SVFUtil::move(symStateOut);
                    return true;
                }
            }
        }
    }

    /// Print buggy path of identified vulnerability
    inline void detailBugReport(const Z3Expr &branchCond, const KeyNodesSet &keyNodesSet) {
        BTPExtractor::detailBugReport(branchCond, keyNodesSet, _curEvalICFGNode, _curEvalSVFGNode, _mainEntry,
                                      _curEvalFuns);
    }

    static inline const std::unique_ptr<ICFGWrapper> &getICFGWrapper() {
        return ICFGWrapper::getICFGWrapper();
    }


    /// Flow functions
    //{%
    /// Flow function for processing merge node
    /// Combine two dataflow facts into a single fact, using set union
    SymState mergeFlowFun(const ICFGNodeWrapper *icfgNodeWrapper, const TypeState &absState,
                          const TypeState &indexAbsState);


    /// Flow function for processing non-branch node
    /// Symbolic state transition when current node contains transition actions (e.g., malloc, free)
    inline void nonBranchFlowFun(const ICFGNodeWrapper *icfgNodeWrapper, SymState &symState) {
        _symStateMgr.setSymState(&symState);
        _symStateMgr.nonBranchFlowFun(icfgNodeWrapper->getICFGNode(), _curEvalSVFGNode);
    }
    //%}


    inline SymState &
    getInfo(const ICFGEdgeWrapper *e, const TypeState &absState, const TypeState &indexAbsState) {
        InfoKey infoKey = std::make_pair(e, absState);
        auto it = _infoMap.find(infoKey);
        if (it == _infoMap.end())
            return *_emptySymState;
        else {
            auto absIt = it->second.find(indexAbsState);
            if (absIt == it->second.end()) {
                return *_emptySymState;
            } else {
                return absIt->second;
            }
        }
    }

    inline bool addInfo(const ICFGEdgeWrapper *e, const TypeState &absState, SymState symState) {
        if (symState.isNullSymState()) return false;
        InfoKey infoKey = std::make_pair(e, absState);
        auto it = _infoMap.find(infoKey);
        if (it != _infoMap.end()) {
            auto absIdxIt = it->second.find(symState.getAbstractState());
            if (absIdxIt == it->second.end()) {
                it->second[symState.getAbstractState()] = SVFUtil::move(symState);
                return true;
            } else {
                SymStates symStates{absIdxIt->second}; // original symstate
                symStates.push_back(SVFUtil::move(symState));
                SymState symStateOut;
                if (groupingAbsStates(symStates, symStateOut)) { // symstate changed
                    absIdxIt->second = SVFUtil::move(symStateOut);
                    return true;
                } else {
                    return false;
                }
            }
        } else {
            _infoMap[infoKey][symState.getAbstractState()] = SVFUtil::move(symState);
            return true;
        }
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
        if (!nodeWrapper) return nullptr;
        const ICFGNode *node = nodeWrapper->getICFGNode();
        if (!nodeWrapper->getRetICFGNodeWrapper()) return nullptr;
        const auto *callBlockNode = SVFUtil::dyn_cast<const CallICFGNode>(node);
        if (!callBlockNode)
            return nullptr;
        // Do not enter callee if current callblocknode is a FSM action
        Set<const SVFFunction *> functionSet;
        _ptaCallgraph->getCallees(callBlockNode, functionSet);
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

    static inline const FunEntryICFGNode *isEntryNode(const ICFGNodeWrapper *node) {
        return SVFUtil::dyn_cast<const FunEntryICFGNode>(node->getICFGNode());
    }

    static inline const FunExitICFGNode *isExitNode(const ICFGNodeWrapper *node) {
        return SVFUtil::dyn_cast<const FunExitICFGNode>(node->getICFGNode());
    }

    static inline bool isMergeNode(const ICFGNodeWrapper *node) {
        u32_t ct = 0;
        for (const auto &edge: node->getInEdges()) {
            if (edge->getICFGEdge()->isIntraCFGEdge())
                ct++;
        }
        return ct > 1;
    }

    static inline bool isBranchNode(const ICFGNodeWrapper *nodeWrapper) {
        u32_t ct = 0;
        for (const auto &edge: nodeWrapper->getICFGNode()->getOutEdges()) {
            if (const IntraCFGEdge *intraCfgEdge = SVFUtil::dyn_cast<IntraCFGEdge>(edge)) {
                if (intraCfgEdge->getCondition())
                    ct++;
            }
        }
        return ct >= 1;
    }
    //%}

    /// Get the symstates from in edges, merge states when multiple in edges
    SymState getSymStateIn(WLItem &curItem);

    /// Initialize info and summary map
    virtual void initMap(SVFModule *module);


    /// Get the incoming intra CFG edges
    static inline Set<const ICFGEdgeWrapper *> getInTEdges(const ICFGNodeWrapper *node) {
        Set<const ICFGEdgeWrapper *> edges;
        for (const auto &edge: node->getInEdges()) {
            if (edge->getICFGEdge()->isIntraCFGEdge())
                edges.insert(edge);
        }
        return SVFUtil::move(edges);
    }


    /// Get the outgoing intra CFG edges
    static inline Set<const ICFGEdgeWrapper *> getOutTEdges(const ICFGNodeWrapper *node) {
        Set<const ICFGEdgeWrapper *> edges;
        for (const auto &edge: node->getOutEdges()) {
            if (edge->getICFGEdge()->isIntraCFGEdge())
                edges.insert(edge);
        }
        return SVFUtil::move(edges);
    }

    /// Print symbolic states of info map and summary map
    virtual void printSS();

    /// The next node to process (return the return icfg node for call icfg node)
    static inline const ICFGNodeWrapper *nextNodeToAdd(const ICFGNodeWrapper *icfgNodeWrapper) {
        if (const CallICFGNode *callICFGNode = SVFUtil::dyn_cast<CallICFGNode>(icfgNodeWrapper->getICFGNode())) {
            if (icfgNodeWrapper->getRetICFGNodeWrapper())
                return icfgNodeWrapper->getRetICFGNodeWrapper();
            else
                return icfgNodeWrapper;
        } else { return icfgNodeWrapper; }
    }

    /// report bug on the current analyzed slice
    virtual void reportBug() = 0;

    virtual void performStat(std::string model);

    /// Get path condition allocator
    static inline BranchAllocator *getPathAllocator() {
        return BranchAllocator::getCondAllocator();
    }



    //%}
}; // end class PSTA


} // end namespace SVF

#endif //PSA_PSTABASE_H
