//
// Created by Xiao on 7/31/2022.
//

#ifndef PSA_BUGREPORTER_H
#define PSA_BUGREPORTER_H

#include "Slicing/GraphSparsificator.h"
#include "Util/Z3Expr.h"
#include "Bases/SymState.h"

namespace SVF {

/**
 * BranchAllocator allocates conditions for each basic block of a certain CFG.
 */
class BranchAllocator {

public:
    typedef Map<u32_t, std::pair<const SVFInstruction *, const SVFInstruction *>> CondIdToTermInstMap;    // map a condition to its branch instruction
    typedef Map<u32_t, const ICFGEdge*> CondIdToICFGEdge;    // map a condition to its branch instruction

private:

    /// Constructor
    BranchAllocator();

    static BranchAllocator* _condAllocator;
    NodeBS _negConds;
    u32_t _totalCondNum;
    CondIdToTermInstMap _condIdToTermInstMap;
    CondIdToICFGEdge _condIdToEdge;

public:

    typedef Z3Expr Condition;

    typedef Map<s64_t, Condition> CondPosMap;        ///< map a branch to its Condition
    typedef Map<const ICFGNode *, CondPosMap> ICFGNodeCondMap; ///< map ICFG node to a Condition

    static inline BranchAllocator* getCondAllocator() {
        if (_condAllocator == nullptr) {
            _condAllocator = new BranchAllocator();
        }
        return _condAllocator;
    }

    static inline void releaseCondAllocator() {
        delete _condAllocator;
        _condAllocator = nullptr;
    }

    /// Destructor
    virtual ~BranchAllocator() {
        icfgNodeConds.clear();
        atomConditions.clear();
    }

    static inline z3::context &getContext() {
        return Z3Expr::getContext();
    }

    static inline Condition getTrueCond() {
        return getContext().bool_val(true);
    }

    static inline Condition getFalseCond() {
        return getContext().bool_val(false);
    }

    static inline z3::check_result solverCheck(const Z3Expr &e) {
        Z3Expr::getSolver().push();
        Z3Expr::getSolver().add(e.getExpr());
        z3::check_result res = Z3Expr::getSolver().check();
        Z3Expr::getSolver().pop();
        return res;
    }

    /// Condition operations
    //@{
    static inline Condition condAnd(const Condition &lhs, const Condition &rhs) {
        return (lhs && rhs).simplify();
    }

    static inline Condition condOr(const Condition &lhs, const Condition &rhs) {
        return (lhs || rhs).simplify();
    }

    static inline Condition condNeg(const Condition &cond) {
        return !cond;
    }

    static Z3Expr condAndLimit(const Z3Expr &lhs, const Z3Expr &rhs);

    static Z3Expr condOrLimit(const Z3Expr &lhs, const Z3Expr &rhs);

    /// Size of the expression
    static u32_t getExprSize(const Z3Expr &lhs);

    /// Iterator every element of the condition
    inline NodeBS exactCondElem(const Condition &cond) const {
        NodeBS elems;
        extractSubConds(cond, elems);
        return elems;
    }

    inline void extractSubConds(const Condition &cond, NodeBS &support) const {
        if (cond.getExpr().num_args() == 1 && isNegCond(cond.id())) {
            support.set(cond.id());
            return;
        }
        if (cond.getExpr().num_args() == 0)
            if (!cond.getExpr().is_true() && !cond.getExpr().is_false())
                support.set(cond.id());
        for (u32_t i = 0; i < cond.getExpr().num_args(); ++i) {
            const z3::expr &expr = cond.getExpr().arg(i);
            extractSubConds(expr, support);
        }
    }

    inline bool isNegCond(u32_t id) const {
        return _negConds.test(id);
    }

    static inline std::string dumpCond(const Condition &cond) {
        std::ostringstream out;
        out << cond.getExpr();
        return out.str();
    }

    /// Allocate a new condition
    inline Condition newCond(const SVFInstruction *inst) {
        u32_t condCountIdx = _totalCondNum++;
        const Condition &cond = getContext().bool_const(("c" + std::to_string(condCountIdx)).c_str());
        const Condition &negCond = condNeg(cond);
        setCondInst(cond, inst);
        setNegCondInst(negCond, inst);
        atomConditions.push_back(cond);
        atomConditions.push_back(negCond);
        return cond;
    }

    inline void setNegCondInst(const Condition &cond, const SVFInstruction *inst) {
        setCondInst(cond, inst);
        _negConds.set(cond.id());
    }

    /// Get/Set llvm conditional expression
    //{@
    inline std::pair<const SVFInstruction *, const SVFInstruction *> getCondInst(u32_t id) const {
        CondIdToTermInstMap::const_iterator it = _condIdToTermInstMap.find(id);
        assert(it != _condIdToTermInstMap.end() && "this should be a fresh condition");
        return it->second;
    }

    inline const ICFGEdge* getConditionalEdge(u32_t id) const {
        auto it = _condIdToEdge.find(id);
        assert(it != _condIdToEdge.end() && "this should be a fresh condition");
        return it->second;
    }

    inline void setCondInst(const Condition &cond, const SVFInstruction *inst) {
        assert(_condIdToTermInstMap.find(cond.id()) == _condIdToTermInstMap.end() &&
               "this should be a fresh condition");
        _condIdToTermInstMap[cond.id()] = std::make_pair(inst, inst);
        _condIdToEdge[cond.id()] = nullptr;
    }
    //@}
    //@}

    /// Perform path allocation
    void allocate();


    /// Print out the path condition information
    void printPathCond();


    /// Get branch condition
    Condition &getBranchCond(const ICFGEdge *edge) const;

    inline u32_t getIcfgNodeCondsNum() const {
        return icfgNodeConds.size();
    }

private:

    /// Allocate path condition for every ICFG Node
    virtual void allocateForICFGNode(const ICFGNode *icfgNode);

    /// Get/Set a branch condition, and its terminator instruction
    //@{
    /// Set branch condition
    void setBranchCond(const ICFGEdge *edge, Condition &cond);

protected:
    ICFGNodeCondMap icfgNodeConds;  ///< map ICFG node to its data and branch conditions
    std::vector<Z3Expr> atomConditions;
};
/*!
 * Report bug
 */
class BTPExtractor {

public:
    typedef SymState::KeyNodes KeyNodes;
    typedef SymState::KeyNodesSet KeyNodesSet;

    BTPExtractor() = default;

    ~BTPExtractor() = default;

    static inline const std::unique_ptr<ICFGWrapper> &getICFGWrapper() {
        return ICFGWrapper::getICFGWrapper();
    }

    static inline PTACallGraph *getPTACallGraph() {
        return AndersenWaveDiff::createAndersenWaveDiff(PAG::getPAG())->getPTACallGraph();
    }

    /// Provide detailed bug report
    static void
    detailBugReport(const Z3Expr &branchCond, const KeyNodesSet &keyNodesSet, const ICFGNode *curEvalICFGNode,
                    const SVFGNode *curEvalSVFGNode, const ICFGNode *mainEntry, Set<const SVFFunction *> &curEvalFuns);

    /// FSM parser singleton
    static inline const std::unique_ptr<FSMParser> &getFSMParser() {
        return FSMParser::getFSMParser();
    }

    /// Bug report
    //{%
    static inline void removeFSMNodeBody(const ICFGNode *curEvalICFGNode, Set<const SVFFunction *> &curEvalFuns) {
        GraphSparsificator::removeFSMNodeBody(curEvalICFGNode, curEvalFuns);
    }

    /// Remove the conditional edge that does not hold (used for bug report)
    static void removeConditionalEdge(Map<const ICFGNode *, Set<const ICFGEdge *>> &conditionalEdges);

    /// Extracting bug-triggering paths
    static void
    extractBTPs(const ICFGNode *curEvalICFGNode, Set<const SVFFunction *> &curEvalFuns, const ICFGNode *mainEntry);
    //%}

    static inline const CallICFGNode *
    isFSMCallNode(const ICFGNodeWrapper *nodeWrapper, const ICFGNode *curEvalICFGNode,
                  Set<const SVFFunction *> &curEvalFuns) {
        return GraphSparsificator::isFSMCallNode(nodeWrapper, curEvalICFGNode, curEvalFuns);
    }

    static inline const CallICFGNode *isExtCall(const ICFGNodeWrapper *nodeWrapper) {
        return GraphSparsificator::isExtCall(nodeWrapper);
    }

    static inline const CallICFGNode *
    isCallNode(const ICFGNodeWrapper *nodeWrapper, const ICFGNode *curEvalICFGNode,
               Set<const SVFFunction *> &curEvalFuns) {
        return TemporalSlicer::isCallNode(nodeWrapper, curEvalICFGNode, curEvalFuns);
    }

    static inline void compactCallNodes(ICFGNodeWrapper *node) {
        GraphSparsificator::compactCallNodes(node);
    }

    static inline void compactIntraNodes(ICFGNodeWrapper *node) {
        GraphSparsificator::compactIntraNodes(node);
    }

    /// Single spatial slicing
    static void spatialSlicing(const SVFGNode *curEvalSVFGNode, const KeyNodes &keyNodes, Set<u32_t> &temporalSlice,
                               Set<u32_t> &spatialSlice, Set<u32_t> &callsites);

    /// Single temporal slicing
    static void
    temporalSlicing(const SVFGNode *curEvalSVFGNode, const KeyNodes &keyNodes, const ICFGNodeWrapper *mainEntry,
                    Set<u32_t> &temporalSlice, Set<const SVFFunction *> &curEvalFuns);

    static inline void clearDF() {
        TemporalSlicer::clearDF();
    }

    static inline void initBUDFTransferFunc(const ICFGNodeWrapper *mainEntry, std::vector<PIState::DataFact> &allDataFacts) {
        TemporalSlicer::initBUDFTransferFunc(mainEntry, allDataFacts);
    }

    static inline void initTDDFTransferFunc(const ICFGNodeWrapper *mainEntry, std::vector<PIState::DataFact> &allDataFacts) {
        TemporalSlicer::initTDDFTransferFunc(mainEntry, allDataFacts);
    }

    /// Top-Down IFDS solver for tailoring
    static inline void
    tdIFDSSolve(const ICFGNodeWrapper *mainEntry, const ICFGNode *curEvalICFGNode,
                Set<const SVFFunction *> &curEvalFuns,
                std::vector<PIState::DataFact> &allDataFacts) {
        TemporalSlicer::tdIFDSSolve(mainEntry, curEvalICFGNode, allDataFacts, curEvalFuns);
    }

    /// Bottom-Up IFDS solver for tailoring
    static inline void
    buIFDSSolve(const ICFGNodeWrapper *mainEntry, const ICFGNodeWrapper *snk, const ICFGNode *curEvalICFGNode,
                Set<const SVFFunction *> &curEvalFuns,
                std::vector<PIState::DataFact> &allDataFacts) {
        TemporalSlicer::buIFDSSolve(mainEntry, snk, curEvalICFGNode, allDataFacts, curEvalFuns);
    }

}; // end class BTPExtractor
} // end namespace SVF



#endif //PSA_BUGREPORTER_H
