//
// Created by Xiao on 2023/7/1.
//

#ifndef TYPESTATE_PSTABASE_H
#define TYPESTATE_PSTABASE_H

#include "Graphs/SVFG.h"
#include "Util/WorkList.h"
#include "WPA/Andersen.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "PSTA/FSMHandler.h"
#include "PSTA/SVFIR2ConsExeState.h"
#include "PSTA/PSAOptions.h"
#include "Bases/SymState.h"
#include "SVF-LLVM/LLVMModule.h"

namespace SVF {
/*!
* Worklist item for ESP
*/
class ESPWLItem {

private:
    const ICFGNode *icfgNode;
    TypeState _typeState;
    TypeState _indexTypeState;
public:
    ESPWLItem(const ICFGNode *_icfgNode, TypeState _absState, TypeState indexAbsState)
            : icfgNode(
            _icfgNode), _typeState(SVFUtil::move(_absState)), _indexTypeState(SVFUtil::move(indexAbsState)) {

    }

    virtual ~ESPWLItem() = default;

    ESPWLItem(const ESPWLItem &wlItem) : icfgNode(wlItem.getICFGNode()),
                                         _typeState(wlItem.getTypeState()),
                                         _indexTypeState(wlItem.getIndexTypeState()) {

    }

    const ICFGNode *getICFGNode() const {
        return icfgNode;
    }

    const TypeState &getTypeState() const {
        return _typeState;
    }

    const TypeState &getIndexTypeState() const {
        return _indexTypeState;
    }

    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator<(const ESPWLItem &rhs) const {
        if (icfgNode != rhs.getICFGNode())
            return icfgNode < rhs.getICFGNode();
        else if (_typeState != rhs.getTypeState())
            return _typeState < rhs.getTypeState();
        else
            return _indexTypeState < rhs.getIndexTypeState();
    }

    inline ESPWLItem &operator=(const ESPWLItem &rhs) {
        if (*this != rhs) {
            icfgNode = rhs.getICFGNode();
            _typeState = rhs.getTypeState();
            _indexTypeState = rhs.getIndexTypeState();
        }
        return *this;
    }

    /// Overloading Operator==
    inline bool operator==(const ESPWLItem &rhs) const {
        return (icfgNode == rhs.getICFGNode()) && (_typeState == rhs.getTypeState()) &&
               _indexTypeState == rhs.getIndexTypeState();
    }

    /// Overloading Operator!=
    inline bool operator!=(const ESPWLItem &rhs) const {
        return !(*this == rhs);
    }
};// end class ESPWLItem



/*!
* Execution State Manager
*/
class ExeStateManager {

private:

    static ExeStateManager *_exeStateMgr;
    SVFIR2ConsExeState translator;
    ConsExeState *_es{nullptr};
    ConsExeState *_globalES{nullptr};
    Set<u32_t> _globalStore;
    Map<const SVFFunction *, OrderedSet<u32_t>> _funcToGlobalPtrs;

    /// Constructor
    explicit ExeStateManager() {}

public:
    /// Singleton
    static inline ExeStateManager *getExeStateMgr() {
        if (_exeStateMgr == nullptr) {
            _exeStateMgr = new ExeStateManager();
        }
        return _exeStateMgr;
    }

    static inline void releaseExeStateManager() {
        delete _exeStateMgr;
        _exeStateMgr = nullptr;
    }

    virtual ~ExeStateManager();

    ConsExeState &getOrBuildGlobalExeState(GlobalICFGNode *node);

    void handleGlobalNode();

    ConsExeState extractGlobalExecutionState();

    static ConsExeState getInitExeState();

    static ConsExeState nullExeState();

    inline void setEs(ConsExeState *es) {
        _es = es;
        translator.setEs(es);
    }

    void moveToGlobal();

    virtual void handleNonBranch(const ICFGNode *node);

    virtual bool handleBranch(const IntraCFGEdge *edge);

    void collectFuncToGlobalPtrs(ICFG *icfg);

    void collectGlobalStore();

}; // end class ExeStateManager

class SymStateManager {
public:
    typedef FSMParser::FSMAction FSMAction;
    typedef std::vector<SymState> SymStates;
    typedef Map<TypeState, const SymState *> AbsStateToSymStateRefMap;
    typedef SymState::KeyNodesSet KeyNodesSet;
    typedef SymState::KeyNodes KeyNodes;
    typedef SVFIR2ConsExeState::Addrs Addrs;

    enum PC_TYPE {
        UNK_PC = 0,
        TRUE_PC,
        FALSE_PC
    };

private:
    const ICFGNode *_curEvalICFGNode{};
    Set<const SVFFunction *> _curEvalFuns;
    Set<FSMParser::CHECKER_TYPE> _checkerTypes;
    SymState *_symState{};
    u32_t _src{0};
    s64_t _srcNum{0};

public:

    /// Constructor
    SymStateManager() = default;

    ~SymStateManager() = default;


    /// Singleton of execution state manager
    static ExeStateManager *getExeStateMgr() {
        return ExeStateManager::getExeStateMgr();
    }

    /// Singleton of execution state manager
    static inline const std::unique_ptr<FSMHandler> &getAbsTransitionHandler() {
        return FSMHandler::getAbsTransitionHandler();
    }

    inline void setSymState(SymState *symState) {
        _symState = symState;
    }

    /// Flow function for processing branch node
    bool branchFlowFun(const IntraCFGEdge *intraEdge, PC_TYPE pcType);

    /// Flow function for processing non-branch node
    void nonBranchFlowFun(const ICFGNode *icfgNode, const SVFGNode *svfgNode);

    void preNonBranchFlowFun(const ICFGNode *icfgNode, const SVFGNode *svfgNode);

    void onTheFlyNonBranchFlowFun(const ICFGNode *icfgNode, const SVFGNode *svfgNode);

    /// Grouping function (property simulation)
    /// @param symStates the first element in symStates is the original symState
    /// @param symStatesOut the resulting grouped symstate
    static bool groupingAbsStates(const SymStates &symStates, SymState &symStateOut);


    /// Global execution state after processing global ICFG node
    static ConsExeState &getOrBuildGlobalExeState(GlobalICFGNode *node) {
        return getExeStateMgr()->getOrBuildGlobalExeState(node);
    }

    /// Global execution state after processing global ICFG node
    static ConsExeState extractGlobalExecutionState() {
        return getExeStateMgr()->extractGlobalExecutionState();
    }

    /// Init execution state with a true path constraint and empty tables
    static ConsExeState getInitExeState() {
        return getExeStateMgr()->getInitExeState();
    }

    /// Singleton of FSM parser
    static const std::unique_ptr<FSMParser> &getFSMParser() {
        return FSMParser::getFSMParser();
    }

    /// Extract the abstract states in current symbolic states
    static inline void extractAbsStatesOfSymStates(const SymStates &ss, Set<TypeState> &absStates) {
        for (const auto &s: ss) {
            absStates.insert(s.getAbstractState());
        }
    }

    /// Whether two symbolic states are equal
    static bool isSupEqSymStates(const SymStates &pre, const SymStates &nxt);

    /// Map _typeState to its symbolic state address (property simulation)
    static inline void mapAbsStateToSymStateRef(const SymStates &symStates, AbsStateToSymStateRefMap &mp) {
        for (const auto &s: symStates) {
            assert(!mp.count(s.getAbstractState()) && "absstate has more than one symstate?");
            mp[s.getAbstractState()] = &s;
        }
    }

    static inline PTACallGraph *getPTACallGraph() {
        return AndersenWaveDiff::createAndersenWaveDiff(PAG::getPAG())->getPTACallGraph();
    }

    inline const CallICFGNode *
    isNonSrcFSMCallNode(const ICFGNode *node, FSMAction &action) {
        if (!node) return nullptr;
        const auto *callBlockNode = SVFUtil::dyn_cast<const CallICFGNode>(node);
        if (!callBlockNode)
            return nullptr;
        // Do not enter callee if current callblocknode is a FSM action
        Set<const SVFFunction *> functionSet;
        getPTACallGraph()->getCallees(callBlockNode, functionSet);
        if (functionSet.empty())
            return nullptr;
        action = getFSMParser()->getTypeFromStr((*functionSet.begin())->getName());
        if ((getFSMParser()->getFSMActions().count(action) && action != FSMParser::CK_DUMMY)) {
            return callBlockNode;
        } else {
            if (getAbsTransitionHandler()->getICFGAbsTransferMap().count(node)) {
                action = getAbsTransitionHandler()->getTypeFromFunc(*functionSet.begin());
                return callBlockNode;
            } else {
                return nullptr;
            }
        }
    }


    void setCurEvalICFGNode(const ICFGNode *icfgNode);

    inline void setCurEvalFuns(Set<const SVFFunction *> &curEvalFuns) {
        _curEvalFuns = curEvalFuns;
    }

    inline void setCheckerTypes(Set<FSMParser::CHECKER_TYPE> &checkerTypes) {
        _checkerTypes = checkerTypes;
    }
}; // end class SymStateManager

/*!
 * Path-sensitive typestate analysis
 */
class PSTABase {
    friend class PSAStat;

public:
    typedef Map<const SVFFunction *, Set<const SVFBasicBlock *>> FunToExitBBsMap;  ///< map a function to all its basic blocks calling program exit
    typedef PTACallGraph::FunctionSet FunctionSet;
    typedef SymStateManager::SymStates SymStates;
    typedef std::pair<const ICFGEdge *, TypeState> InfoKey; ///< a pair of ICFG edge and abstract state
    typedef FIFOWorkList<ESPWLItem> WorkList;
    typedef Map<TypeState, SymState> AbsToSymState;
    typedef Map<InfoKey, AbsToSymState> InfoMap;
    typedef Map<TypeState, SymStates> AbsStateToSymStatesMap;        ///< map abstract state to symbolic states
    typedef FSMHandler::SrcSet SrcSet;
    typedef FSMHandler::ICFGNodeSet ICFGNodeSet;
    typedef SymStateManager::PC_TYPE PC_TYPE;
    typedef std::pair<const SVFFunction *, TypeState> SummaryKey; ///< a pair of function and abstract state
    typedef Map<SummaryKey, AbsToSymState> SummaryMap;
    typedef SymState::KeyNodesSet KeyNodesSet;
    typedef SymState::KeyNodes KeyNodes;
    typedef std::vector<const SVFVar *> SVFVarVector;


protected:
    ICFG *_icfg{nullptr};
    PTACallGraph *_ptaCallgraph{nullptr};
    const ICFGNode *_mainEntry{nullptr};
    const SVFFunction *_mainFunc{nullptr};
    SymStateManager _symStateMgr;
    InfoMap _infoMap;
    SrcSet _srcs;
    ICFGNodeSet _snks;                    ///< map source object (SVFGNode) to its sinks (ICFGNode set)
    SymState *_emptySymState;
    WorkList _workList;
    const SVFGNode *_curEvalSVFGNode{nullptr};
    const ICFGNode *_curEvalICFGNode{nullptr};
    Set<const SVFFunction *> _curEvalFuns;
    FunToExitBBsMap _funToExitBBsMap;  ///< map a function to all its basic blocks calling program exit
    SummaryMap _summaryMap;
    std::string _fsmFile;
public:
    /// Constructor
    PSTABase();

    /// Destructor
    virtual ~PSTABase();

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
    virtual void processCallNode(const CallICFGNode *callBlockNode, ESPWLItem &wlItem);


    /// Process Exit Node
    virtual void processExitNode(ESPWLItem &wlItem);


    /// Process Branch Node
    virtual void processBranchNode(ESPWLItem &wlItem);

    /// Process Other Node
    virtual void processOtherNode(ESPWLItem &wlItem);
    //%}

    virtual inline bool addTrigger(const ICFGEdge *e, SymState symState) {
        TypeState curAbsState = symState.getAbstractState();
        return addInfo(e, curAbsState, SVFUtil::move(symState));
    }

    virtual inline bool
    addToSummary(const ICFGNode *node, const TypeState &absState, SymState &symState) {
        if (symState.isNullSymState()) return false;
        SummaryKey summaryKey = std::make_pair(fn(node), absState);
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

    virtual inline const SymState &
    getSummary(const SVFFunction *fun, const TypeState &absState, const TypeState &indexAbsState) {
        SummaryKey summaryKey = std::make_pair(fun, absState);
        auto it = _summaryMap.find(summaryKey);
        if (it == _summaryMap.end())
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

    static inline SymState applySummary(SymState &symState, const SymState &summary) {
        SymState symStateOut(symState.getExecutionState(), summary.getAbstractState());
        symStateOut.getExecutionState().applySummary(summary.getExecutionState());
        if (PSAOptions::EnableReport()) {
            symStateOut.setBranchCondition(summary.getBranchCondition());
            symStateOut.setKeyNodesSet(SVFUtil::move(summary.getKeyNodesSet()));
        }
        return SVFUtil::move(symStateOut);
    }

    virtual inline bool hasSummary(const SVFFunction *fun, const TypeState &absState) {
        for (const auto &as: getFSMParser()->getAbsStates()) {
            const SymState &summary = getSummary(fun, absState, as);
            if (!summary.isNullSymState()) {
                return true;
            }
        }
        return false;
    }

    static inline SymState
    extractToSummarySymState(SymState &symStateSummary, const SVFVar *formalRet,
                             Set<const FormalOUTSVFGNode *> &formalOuts) {
        ConsExeState es(ExeStateManager::getInitExeState());
        buildSummary(es, const_cast<ConsExeState &>(symStateSummary.getExecutionState()), formalRet, formalOuts);
        SymState newS(SVFUtil::move(es), symStateSummary.getAbstractState());
        if (PSAOptions::EnableReport()) {
            newS.setBranchCondition(symStateSummary.getBranchCondition());
            newS.setKeyNodesSet(SVFUtil::move(const_cast<KeyNodesSet &>(symStateSummary.getKeyNodesSet())));
        }
        return SVFUtil::move(newS);
    }

    static inline Set<const FormalINSVFGNode *> getFormalInSVFGNodes(const FunEntryICFGNode *entryNode) {
        Set<const FormalINSVFGNode *> res;
        for (const auto &vfNode: entryNode->getVFGNodes()) {
            if (const FormalINSVFGNode *formalIn = SVFUtil::dyn_cast<FormalINSVFGNode>(vfNode)) {
                res.insert(formalIn);
            }
        }
        return std::move(res);
    }

    static inline Set<const FormalOUTSVFGNode *> getFormalOutSVFGNodes(const FunExitICFGNode *exitNode) {
        Set<const FormalOUTSVFGNode *> res;
        for (const auto &vfNode: exitNode->getVFGNodes()) {
            if (const FormalOUTSVFGNode *formalOut = SVFUtil::dyn_cast<FormalOUTSVFGNode>(vfNode)) {
                res.insert(formalOut);
            }
        }
        return std::move(res);
    }


    /// FSM parser singleton
    static inline const std::unique_ptr<FSMParser> &getFSMParser() {
        return FSMParser::getFSMParser();
    }

    /// AbsTransitionHandler singleton
    static inline const std::unique_ptr<FSMHandler> &getAbsTransitionHandler() {
        return FSMHandler::getAbsTransitionHandler();
    }


    static inline ExeStateManager *getExeStateMgr() {
        return ExeStateManager::getExeStateMgr();
    }

    /// Flow functions
    //{%
    /// Flow function for processing merge node
    /// Combine two dataflow facts into a single fact, using set union
    SymState mergeFlowFun(const ICFGNode *icfgNode, const TypeState &absState,
                          const TypeState &indexAbsState);

    /// Flow function for processing branch node
    inline void branchFlowFun(SymState &symState, const IntraCFGEdge *intraEdge, PC_TYPE pcType) {
        _symStateMgr.setSymState(&symState);
        if (!_symStateMgr.branchFlowFun(intraEdge, pcType))
            symState = *_emptySymState;
    }

    /// Flow function for processing non-branch node
    /// Symbolic state transition when current node contains transition actions (e.g., malloc, free)
    inline void nonBranchFlowFun(const ICFGNode *icfgNode, SymState &symState) {
        _symStateMgr.setSymState(&symState);
        _symStateMgr.nonBranchFlowFun(icfgNode, _curEvalSVFGNode);
    }
    //%}

    /// Grouping function (property simulation)
    /// @param symStates the first element in symStates is the original symState
    /// @param symStatesOut the resulting grouped symstate
    static inline bool groupingAbsStates(const SymStates &symStates, SymState &symStateOut) {
        return SymStateManager::groupingAbsStates(symStates, symStateOut);
    }

    /// Map _typeState to its symbolic states (property simulation)
    static inline void mapAbsStateToSymStates(const SymStates &symStates, AbsStateToSymStatesMap &mp) {
        for (const auto &s: symStates)
            mp[s.getAbstractState()].push_back(SVFUtil::move(const_cast<SymState &>(s)));
    }

    inline SymState &
    getInfo(const ICFGEdge *e, const TypeState &absState, const TypeState &indexAbsState) {
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

    inline bool addInfo(const ICFGEdge *e, const TypeState &absState, SymState symState) {
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
    static inline const SVFFunction *fn(const ICFGNode *node) {
        return node->getFun();
    }

    /// Maps a function name to its entry node
    inline const ICFGNode *entryNode(const SVFFunction *svfFunction) {
        return _icfg->getFunEntryICFGNode(svfFunction);
    }

    /// Maps a call node to the name of the called function
    static inline void callee(const CallICFGNode *callBlockNode, FunctionSet &funSet) {
        for (const auto &edge: callBlockNode->getOutEdges()) {
            if (edge->isCallCFGEdge())
                funSet.insert(edge->getDstNode()->getFun());
        }
    }

    /// Maps a return-site node to its call-site node
    inline const ICFGNode *callSite(const ICFGNode *retBlockNode) {
//        return retBlockNode->getCallICFGNodeWrapper();
        return _icfg->getICFGNode(SVFUtil::dyn_cast<RetICFGNode>(retBlockNode)->getCallSite());
    }

    /// Maps an exit node to its return-site nodes
    static inline void
    returnSites(const ICFGNode *funExitBlockNode, std::vector<const ICFGNode *> &toRet) {
        for (const auto &edge: funExitBlockNode->getOutEdges()) {
            if (edge->isRetCFGEdge()) {
                const RetICFGNode *retBlockNode = SVFUtil::dyn_cast<RetICFGNode>(edge->getDstNode());
                assert(retBlockNode && "not return site?");
                toRet.push_back(edge->getDstNode());
            }
        }
    }

    /// Judge node type
    //{%
    inline const CallICFGNode *isCallNode(const ICFGNode *node) const {
        if (!node) return nullptr;
//        const ICFGNode *node = node->getICFGNode();
        if (!SVFUtil::isa<CallICFGNode>(node)) return nullptr;
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

    static inline const FunEntryICFGNode *isEntryNode(const ICFGNode *node) {
        return SVFUtil::dyn_cast<const FunEntryICFGNode>(node);
    }

    static inline const FunExitICFGNode *isExitNode(const ICFGNode *node) {
        return SVFUtil::dyn_cast<const FunExitICFGNode>(node);
    }

    static inline bool isMergeNode(const ICFGNode *node) {
        u32_t ct = 0;
        for (const auto &edge: node->getInEdges()) {
            if (edge->isIntraCFGEdge())
                ct++;
        }
        return ct > 1;
    }

    static inline bool isBranchNode(const ICFGNode *node) {
        u32_t ct = 0;
        for (const auto &edge: node->getOutEdges()) {
            if (const IntraCFGEdge *intraCfgEdge = SVFUtil::dyn_cast<IntraCFGEdge>(edge)) {
                if (intraCfgEdge->getCondition())
                    ct++;
            }
        }
        return ct >= 1;
    }
    //%}

    /// Get the symstates from in edges, merge states when multiple in edges
    SymState getSymStateIn(ESPWLItem &curItem);

    /// Initialize info and summary map
    virtual void initMap(SVFModule *module);

    /// Get the incoming intra CFG edges
    static inline Set<const ICFGEdge *> getInTEdges(const ICFGNode *node) {
        Set<const ICFGEdge *> edges;
        for (const auto &edge: node->getInEdges()) {
            if (edge->isIntraCFGEdge())
                edges.insert(edge);
        }
        return edges;
    }


    /// Get the outgoing intra CFG edges
    static inline Set<const ICFGEdge *> getOutTEdges(const ICFGNode *node) {
        Set<const ICFGEdge *> edges;
        for (const auto &edge: node->getOutEdges()) {
            if (edge->isIntraCFGEdge())
                edges.insert(edge);
        }
        return SVFUtil::move(edges);
    }

    /// The next node to process (return the return icfg node for call icfg node)
    static inline const ICFGNode *nextNodeToAdd(const ICFGNode *icfgNode) {
        if (const CallICFGNode *callICFGNode = SVFUtil::dyn_cast<CallICFGNode>(icfgNode)) {
            if (callICFGNode->getRetICFGNode())
                return callICFGNode->getRetICFGNode();
            else
                return callICFGNode;
        } else { return icfgNode; }
    }

    /// report bug on the current analyzed slice
    virtual void reportBug() = 0;

//        virtual void performStat(std::string model);

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

    inline LLVMModuleSet *getLLVMModuleSet() {
        return LLVMModuleSet::getLLVMModuleSet();
    }

    /// The physical address starts with 0x7f...... + idx
    static inline u32_t getVirtualMemAddress(u32_t idx) {
        return AddressValue::getVirtualMemAddress(idx);
    }

    /// Build symstates for add trigger
    static void buildTrigger(ConsExeState &es, const SVFVarVector &formalVs, Set<const FormalINSVFGNode *> &formalIns);

    /// Build summary based on side-effects and formal return
    static void buildSummary(ConsExeState &es, ConsExeState &summary, const SVFVar *formalRet,
                             Set<const FormalOUTSVFGNode *> &formalOuts);

    /// Evaluate null like expression for source-sink related bug detection in SABER
    PC_TYPE evaluateTestNullLikeExpr(const BranchStmt *cmpInst, const IntraCFGEdge *edge);

    /// Evaluate loop exit branch
    PC_TYPE evaluateLoopExitBranch(const SVFBasicBlock *bb, const SVFBasicBlock *succ);

    /// Return condition when there is a branch calls program exit
    PC_TYPE evaluateProgExit(const BranchStmt *brInst, const SVFBasicBlock *succ);

    PC_TYPE evalBranchCond(const IntraCFGEdge *intraCfgEdge);

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

    static bool isSrcSnkReachable(const ConsExeState &src, const ConsExeState &snk);
}; // end class PSTABase

}
/// Specialized hash for ESPWLItem.
template<>
struct std::hash<SVF::ESPWLItem> {
    size_t operator()(const SVF::ESPWLItem &wlItem) const {
        SVF::Hash<std::pair<SVF::TypeState, std::pair<const SVF::ICFGNode *, SVF::TypeState>>> h;
        return h(std::make_pair(wlItem.getIndexTypeState(),
                                std::make_pair(wlItem.getICFGNode(), wlItem.getTypeState())));
    }
};

#endif //TYPESTATE_PSTABASE_H
