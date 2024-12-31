//
// Created by Xiao on 2022/4/3.
//

#ifndef PSA_SQEXTRACTOR_H
#define PSA_SQEXTRACTOR_H


#include <WPA/Andersen.h>
#include "SVF-LLVM/LLVMUtil.h"
#include "Slicing/PIState.h"
#include "Slicing/ICFGWrapper.h"
#include "SVF-LLVM/LLVMModule.h"

/*!
 * In this file, we have class:
 * Worklist item for ESP: WLItem
 * Operation Sequences worklist item: SQWLItem
 * Worklist item for temporal slicing: TPSIFDSItem
 * Operation Sequences (PI) Extractor:
 *
 */

namespace SVF {
class WLItem;

/*!
 * Worklist item for PSTA
 */
class WLItem {

private:
    const ICFGNodeWrapper *icfgNodeWrapper;
    TypeState _typeState;
    TypeState _indexTypeState;
public:
    WLItem(const ICFGNodeWrapper *_icfgNodeWrapper, TypeState _absState, TypeState indexAbsState)
            : icfgNodeWrapper(
            _icfgNodeWrapper), _typeState(SVFUtil::move(_absState)), _indexTypeState(SVFUtil::move(indexAbsState)) {

    }

    virtual ~WLItem() = default;

    WLItem(const WLItem &wlItemWrapper) : icfgNodeWrapper(wlItemWrapper.getICFGNodeWrapper()),
                                          _typeState(wlItemWrapper.getTypeState()),
                                          _indexTypeState(wlItemWrapper.getIndexTypeState()) {

    }

    const ICFGNodeWrapper *getICFGNodeWrapper() const {
        return icfgNodeWrapper;
    }

    const TypeState &getTypeState() const {
        return _typeState;
    }

    const TypeState &getIndexTypeState() const {
        return _indexTypeState;
    }

    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator<(const WLItem &rhs) const {
        if (icfgNodeWrapper != rhs.getICFGNodeWrapper())
            return icfgNodeWrapper < rhs.getICFGNodeWrapper();
        else if (_typeState != rhs.getTypeState())
            return _typeState < rhs.getTypeState();
        else
            return _indexTypeState < rhs.getIndexTypeState();
    }

    inline WLItem &operator=(const WLItem &rhs) {
        if (*this != rhs) {
            icfgNodeWrapper = rhs.getICFGNodeWrapper();
            _typeState = rhs.getTypeState();
            _indexTypeState = rhs.getIndexTypeState();
        }
        return *this;
    }

    /// Overloading Operator==
    inline bool operator==(const WLItem &rhs) const {
        return (icfgNodeWrapper == rhs.getICFGNodeWrapper()) && (_typeState == rhs.getTypeState()) &&
               _indexTypeState == rhs.getIndexTypeState();
    }

    /// Overloading Operator!=
    inline bool operator!=(const WLItem &rhs) const {
        return !(*this == rhs);
    }
};// end class WLItem



class SQWLItem {

private:
    const ICFGNodeWrapper *_icfgNodeWrapper;
    TypeState _typeState;
public:
    SQWLItem(const ICFGNodeWrapper *icfgNodeWrapper, TypeState absState)
            : _icfgNodeWrapper(
            icfgNodeWrapper), _typeState(SVFUtil::move(absState)) {

    }

    virtual ~SQWLItem() = default;

    SQWLItem(const WLItem &wlItemWrapper) : _icfgNodeWrapper(wlItemWrapper.getICFGNodeWrapper()),
                                            _typeState(wlItemWrapper.getTypeState()) {

    }

    const ICFGNodeWrapper *getICFGNodeWrapper() const {
        return _icfgNodeWrapper;
    }

    const TypeState &getTypeState() const {
        return _typeState;
    }


    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator<(const SQWLItem &rhs) const {
        if (_icfgNodeWrapper != rhs.getICFGNodeWrapper())
            return _icfgNodeWrapper < rhs.getICFGNodeWrapper();
        else
            return _typeState < rhs.getTypeState();
    }

    inline SQWLItem &operator=(const SQWLItem &rhs) {
        if (*this != rhs) {
            _icfgNodeWrapper = rhs.getICFGNodeWrapper();
            _typeState = rhs.getTypeState();
        }
        return *this;
    }

    /// Overloading Operator==
    inline bool operator==(const SQWLItem &rhs) const {
        return (_icfgNodeWrapper == rhs.getICFGNodeWrapper()) && (_typeState == rhs.getTypeState());
    }

    /// Overloading Operator!=
    inline bool operator!=(const SQWLItem &rhs) const {
        return !(*this == rhs);
    }
};// end class WLItem


/*!
 * Worklist item for temporal slicing
 */
class TPSIFDSItem {
public:
    typedef PIState::DataFact DataFact;
    typedef std::pair<const ICFGNodeWrapper *, DataFact> NodeDFPair;
private:
    NodeDFPair src;
    NodeDFPair dst;
public:
    TPSIFDSItem(NodeDFPair _src, NodeDFPair _dst) : src(SVFUtil::move(_src)), dst(SVFUtil::move(_dst)) {}

    TPSIFDSItem(const ICFGNodeWrapper *_srcICFGNode, const DataFact &_srcDF, const ICFGNodeWrapper *_dstICFGNode,
                const DataFact &_dstDF) {
        src = std::make_pair(_srcICFGNode, _srcDF);
        dst = std::make_pair(_dstICFGNode, _dstDF);
    }

    TPSIFDSItem(const TPSIFDSItem &_ifdsItem) : src(_ifdsItem.getSrc()), dst(_ifdsItem.getDst()) {}

    virtual ~TPSIFDSItem() = default;

    TPSIFDSItem(TPSIFDSItem &&rhs) noexcept: src(SVFUtil::move(rhs.src)), dst(SVFUtil::move(rhs.dst)) {

    }

    TPSIFDSItem &operator=(TPSIFDSItem &&rhs) noexcept {
        if (this != &rhs) {
            src = SVFUtil::move(rhs.src);
            dst = SVFUtil::move(rhs.dst);
        }
        return *this;
    }

    inline NodeDFPair getSrc() const {
        return src;
    }

    inline NodeDFPair getDst() const {
        return dst;
    }

    inline TPSIFDSItem &operator=(const TPSIFDSItem &_ifdsItem) {
        if (*this != _ifdsItem) {
            src = _ifdsItem.getSrc();
            dst = _ifdsItem.getDst();
        }
        return *this;
    }

    inline bool operator<(const TPSIFDSItem &_ifdsItem) const {
        if (src.first->getId() != _ifdsItem.getSrc().first->getId())
            return src.first->getId() < _ifdsItem.getSrc().first->getId();
        else if (src.second.size() != _ifdsItem.getSrc().second.size())
            return src.second.size() < _ifdsItem.getSrc().second.size();
        else if (dst.first->getId() != _ifdsItem.getDst().first->getId())
            return dst.first->getId() < _ifdsItem.getDst().first->getId();
        else
            return dst.second.size() < _ifdsItem.getDst().second.size();
    }

    inline bool operator==(const TPSIFDSItem &_ifdsItem) const {
        return src == _ifdsItem.getSrc() && dst == _ifdsItem.getDst();
    }

    inline bool operator!=(const TPSIFDSItem &_ifdsItem) const {
        return !(*this == _ifdsItem);
    }
};// end class TPSIFDSItem

/*!
 * Operation Sequences (PI) Extractor
 */
class PIExtractor {
public:
    typedef TPSIFDSItem::DataFact DataFact;
    typedef PIState::PI PI;
    typedef Map<const SVFGNode *, PI> SrcToPI;
    typedef FSMHandler::SrcSet SrcSet;
    typedef FSMHandler::ICFGNodeSet ICFGNodeSet;
    typedef Set<u32_t> NodeIDSet;
    typedef Map<const SVFGNode *, NodeIDSet> SrcToNodeIDSetMap;
    typedef std::pair<const SVFFunction *, TypeState> SummaryKey;
    typedef PIStateManager::PIStates PIStates;
    typedef PIStateManager::OrderedPIStates OrderedPIStates;
    typedef Map<TypeState, PIStates> AbsStateToPIStates;
    typedef PTACallGraph::FunctionSet FunctionSet;
    typedef Map<const SVFFunction *, Set<const SVFBasicBlock *>> FunToExitBBsMap;  ///< map a function to all its basic blocks calling program exit
    typedef OrderedMap<TypeState, const PIState *> AbsToPIStateRef;
    typedef OrderedMap<TypeState, PIState> AbsToPIState;
    typedef Map<SummaryKey, AbsToPIState> SummaryMap;

    typedef FIFOWorkList<WLItem> WorkList;

    enum PC_TYPE {
        UNK_PC = 0,
        TRUE_PC,
        FALSE_PC
    };


private:
    PIStateManager _PIStateManager;
    const ICFGNode *_curEvalICFGNode{nullptr};
    const SVFGNode *_curEvalSVFGNode{nullptr};
    Set<const SVFFunction *> _curEvalFuns;
    ICFGNodeSet _snks;
    SummaryMap _summaryMap;
    const ICFGNodeWrapper *_mainEntry{nullptr};
    PIStates _emptyPIStates;
    FunToExitBBsMap _funToExitBBsMap;  ///< map a function to all its basic blocks calling program exit
    PIState _emptyPIState;

public:
    PIExtractor() = default;

    ~PIExtractor() = default;

    static PTACallGraph *getPTACallGraph() {
        return AndersenWaveDiff::createAndersenWaveDiff(PAG::getPAG())->getPTACallGraph();
    }

    void collectKeptNodes(Set<u32_t> &kept);

    void compactICFGWrapper();

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

    void initialize(SVFModule *svfModule);

    /// Given a pattern, extract all the operation sequences for each src using enumeration
    void
    extractSQbyEnumeration(SrcSet &srcs, SrcToPI &srcToSQ, DataFact &pattern);

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
    extract(SVFModule *svfModule, const SVFGNode *src, PI &sQ, Set<FSMParser::CHECKER_TYPE> &checkerTypes,
            const ICFGNodeSet &snks);

    static void collectPI(PI &seqs, OrderedSet<const ICFGNode *> &sks, const ICFGNode *curEvalICFGNode);

    static void collectUpper(const ICFGNodeWrapper *curNode, const ICFGNodeWrapper *evalNode, PI &seqs,
                             const ICFGNode *curEvalICFGNode, Set<const ICFGNodeWrapper *> &visited);

    void initMap(SVFModule *svfModule);

    static void clearMap();

    void solve();

    /// Enumerate PI given separate nodes
    void enumerateSQ(u32_t index, DataFact &pattern, PI &seqs, DataFact &tmp,
                     Map<u32_t, Set<u32_t>> actionToNodes);

    //{% Flow functions
    /// Flow function for processing merge node
    /// Combine two dataflow facts into a single fact, using set union
    inline PIState
    mergeFlowFun(const ICFGNodeWrapper *icfgNodeWrapper, const TypeState &absState,
                 const TypeState &idxAbsState) {
        PIStates sqStatesTmp;
        for (const auto &edge: icfgNodeWrapper->getInEdges()) {
            if (edge->getICFGEdge()->isIntraCFGEdge()) {
                const PIState& sqState = getInfo(edge, absState, idxAbsState);
                if (&sqState != &_emptyPIState) {
                    sqStatesTmp.push_back(sqState);
                }
            }
        }
        PIState piStateOut;
        groupingAbsStates(sqStatesTmp, piStateOut);
        return SVFUtil::move(piStateOut);
    }

    /// Flow function for processing non-branch node
    /// Symbolic state transition when current node contains transition actions (e.g., malloc, free)
    inline void nonBranchFlowFun(const ICFGNodeWrapper *icfgNode, PIState *sqState) {
        _PIStateManager.nonBranchFlowFun(icfgNode->getICFGNode(), sqState, _curEvalSVFGNode);
    }
    //%}


    /// Grouping function (property simulation)
    inline bool groupingAbsStates(const PIStates &sqStates, PIState& piStateOut) {
        if (sqStates.size() == 1) {
            piStateOut = std::move(const_cast<PIState &>(*sqStates.begin()));
            return false;
        }
        PI pi = SVFUtil::move(const_cast<PI &>(sqStates.begin()->getPI()));
        bool changed = false;
        for (u32_t i = 1; i < sqStates.size(); ++i) {
            for (const auto &p: sqStates[i].getPI()) {
                if (pi.insert(std::move(const_cast<DataFact&>(p))).second) {
                    changed = true;
                }
            }
        }
        piStateOut = std::move(PIState((*sqStates.begin()).getAbstractState(), std::move(pi)));
        return changed;
    }


    inline bool addInfo(const ICFGEdgeWrapper *e, const TypeState &absState, PIState sqState) {
        if(sqState.isNullPI()) return false;
        auto *edge = const_cast<ICFGEdgeWrapper *>(e);
        auto it = edge->_piInfoMap.find(absState);
        if (it == edge->_piInfoMap.end()) {
            edge->_piInfoMap[absState][sqState.getAbstractState()] = SVFUtil::move(sqState);
            return true;
        } else {
            auto absIt = it->second.find(sqState.getAbstractState());
            if (absIt == it->second.end()) {
                it->second[sqState.getAbstractState()] = SVFUtil::move(sqState);
                return true;
            } else {
                PIStates sqStates{absIt->second};
                sqStates.push_back(SVFUtil::move(sqState));
                PIState piStateOut;
                if (groupingAbsStates(sqStates, piStateOut)) {
                    absIt->second = SVFUtil::move(piStateOut);
                    return true;
                } else {
                    return false;
                }
            }
        }
    }

    inline PIState&
    getInfo(ICFGEdgeWrapper *e, const TypeState &absState, const TypeState &idxAbsState) {
        auto it = e->_piInfoMap.find(absState);
        if (it == e->_piInfoMap.end()) {
            return _emptyPIState;
        } else {
            auto absIt = it->second.find(idxAbsState);
            if (absIt == it->second.end()) {
                return _emptyPIState;
            } else {
                return absIt->second;
            }
        }
    }

    virtual inline bool addTrigger(const ICFGEdgeWrapper *e, PIState sqState) {
        TypeState curAbsState = sqState.getAbstractState();
        return addInfo(e, curAbsState, SVFUtil::move(sqState));
    }

    inline bool
    addTrigger(const ICFGNodeWrapper *node, PIState sqState) {
        assert(!node->getOutEdges().empty() && "PIExtractor::addTrigger");
        const ICFGEdgeWrapper *edge = *node->getOutEdges().begin();
        TypeState curAbsState = sqState.getAbstractState();
        return addInfo(edge, curAbsState, std::move(sqState));
    }

    inline bool addToSummary(const ICFGNodeWrapper *node, const TypeState &absState, PIState& sqState) {
        if (sqState.isNullPI()) return false;
        SummaryKey summaryKey = std::make_pair(fn(node), absState);
        auto it = _summaryMap.find(summaryKey);
        if (it != _summaryMap.end()) {
            auto absIdxIt = it->second.find(sqState.getAbstractState());
            if (absIdxIt == it->second.end()) {
                it->second[sqState.getAbstractState()] = std::move(sqState);
                return true;
            } else {
                PIStates sqStates{absIdxIt->second};
                sqStates.push_back(std::move(sqState));
                PIState piStateOut;
                if (groupingAbsStates(sqStates, piStateOut)) {
                    absIdxIt->second = std::move(piStateOut);
                    return true;
                } else{
                    return false;
                }
            }
        } else {
            _summaryMap[summaryKey][sqState.getAbstractState()] = std::move(sqState);
        }
        return true;
    }

    inline PIState applySummary(PIState &curState, const PIState &summary) {
        const PI &curSQ = curState.getPI();
        PI newSQ;
        if (curSQ.empty()) {
            newSQ.insert(summary.getPI().begin(), summary.getPI().end());
        } else {
            for (const auto &sq: curSQ) {
                if (summary.getPI().empty()) {
                    newSQ.insert(sq);
                } else {
                    for (const auto &sqSummary: summary.getPI()) {
                        DataFact newseq(sq);
                        newseq.insert(newseq.end(), sqSummary.begin(), sqSummary.end());
                        newSQ.insert(SVFUtil::move(newseq));
                    }
                }
            }
        }
        return std::move(PIState(summary.getAbstractState(), std::move(newSQ)));
    }

    inline PIState&
    getSummary(const SVFFunction *fun, const TypeState &absState, const TypeState &idxAbsState) {
        SummaryKey summaryKey = std::make_pair(fun, absState);
        auto it = _summaryMap.find(summaryKey);
        if (it == _summaryMap.end())
            return _emptyPIState;
        else {
            auto absIt = it->second.find(idxAbsState);
            if (absIt == it->second.end()) {
                return _emptyPIState;
            } else {
                return absIt->second;
            }
        }
    }

    inline bool hasSummary(const SVFFunction *fun, const TypeState &absState) {
        for (const auto &as: getFSMParser()->getAbsStates()) {
            if (!getSummary(fun, absState, as).isNullPI())
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


/// Specialized hash for TPSIFDSItem
template<>
struct std::hash<SVF::TPSIFDSItem> {
    std::size_t operator()(const SVF::TPSIFDSItem &cdpi) const {
        SVF::Hash<std::pair<SVF::TPSIFDSItem::NodeDFPair, SVF::TPSIFDSItem::NodeDFPair>> h;
        return h(std::make_pair(cdpi.getSrc(), cdpi.getDst()));
    }
};

/// Specialized hash for WLItem.
template<>
struct std::hash<SVF::WLItem> {
    size_t operator()(const SVF::WLItem &wlItemWrapper) const {
        SVF::Hash<std::pair<SVF::TypeState, std::pair<const SVF::ICFGNodeWrapper *, SVF::TypeState>>> h;
        return h(std::make_pair(wlItemWrapper.getIndexTypeState(),
                                std::make_pair(wlItemWrapper.getICFGNodeWrapper(), wlItemWrapper.getTypeState())));
    }
};

/// Specialized hash for SQWLItem.
template<>
struct std::hash<SVF::SQWLItem> {
    size_t operator()(const SVF::SQWLItem &wlItemWrapper) const {
        SVF::Hash<std::pair<const SVF::ICFGNodeWrapper *, SVF::TypeState>> h;
        return h(std::make_pair(wlItemWrapper.getICFGNodeWrapper(), wlItemWrapper.getTypeState()));
    }
};

#endif //PSA_SQEXTRACTOR_H
