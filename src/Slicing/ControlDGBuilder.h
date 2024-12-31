/*
 * CDBuilder.h
 *
 * Ron Cytron, Jeanne Ferrante, Barry K. Rosen, Mark N. Wegman, and F. Kenneth Zadeck. 1991.
 * "Efficiently computing static single assignment form and the control dependence graph."
 * ACM Transactions on Programming Languages and Systems (TOPLAS) 13, 4 (1991), 451–490.
 * https://dl.acm.org/doi/10.1145/115372.115320
 *
 * Created by xiao on 3/21/22.
 *
 */
#ifndef CDBUILDER_H
#define CDBUILDER_H

#include "SVF-LLVM/LLVMUtil.h"
#include "ControlDG.h"
#include "SVFIR/SVFValue.h"
// control dependence builder
namespace SVF {
class ControlDGBuilder {
public:

    /// constructor
    ControlDGBuilder() : _controlDG(ControlDG::getControlDG()) {

    }

    /// destructor
    ~ControlDGBuilder() {

    }

    /// start here
    void build();

    /// build control dependence for each function
    void buildControlDependence(const SVFModule *svfgModule);

    /// build BB to ICFG nodes
    void buildBBtoICFGNodes(const ICFG *icfg);

    /// build map at icfg node level
    void buildICFGNodeControlMap();

    /// whether bb has controlling/dependent BBs
    //{@
    inline bool hasControlBBs(const BasicBlock *bb) {
        return _controlMap.count(bb);
    }

    inline bool hasDependentOnBBs(const BasicBlock *bb) {
        return _dependentOnMap.count(bb);
    }
    //@}

    /// get the BBs that bb controls/is dependent on
    //{@
    inline const Map<const BasicBlock *, Set<s32_t>> &getControlBBs(const BasicBlock *bb) {
        assert(hasControlBBs(bb) && "basic block not in control map!");
        return _controlMap[bb];
    }

    inline const Map<const BasicBlock *, Set<s32_t>> &getDependentOnBBs(const BasicBlock *bb) {
        assert(hasDependentOnBBs(bb) && "basic block not in control dependent on map!");
        return _dependentOnMap[bb];
    }
    //@}

    /// whether bb has controlling/dependent BBs
    //{@
    inline bool hasControlNodes(const ICFGNode *node) {
        return _nodeControlMap.count(node);
    }

    inline bool hasDependentOnNodes(const ICFGNode *node) {
        return _nodeDependentOnMap.count(node);
    }
    //@}

    /// get the BBs that bb controls/is dependent on
    //{@
    inline const Map<const ICFGNode *, Set<s32_t>> &getControlNodes(const ICFGNode *node) {
        assert(hasControlNodes(node) && "icfg node not in control map!");
        return _nodeControlMap[node];
    }

    inline const Map<const ICFGNode *, Set<s32_t>> &getDependentOnNodes(const ICFGNode *node) {
        assert(hasDependentOnNodes(node) && "icfg node not in control dependent on map!");
        return _nodeDependentOnMap[node];
    }
    //@}

    /// whether lbb controls rbb
    inline bool control(const BasicBlock *lbb, const BasicBlock *rbb) {
        if (!hasControlBBs(lbb))
            return false;
        return _controlMap[lbb].count(rbb);
    }

    /// whether lnode controls rnode
    inline bool control(const ICFGNode *lnode, const ICFGNode *rnode) {
        if (!hasControlNodes(lnode))
            return false;
        return _nodeControlMap[lnode].count(rnode);
    }

    u32_t getBBSuccessorPos(const BasicBlock *BB, const BasicBlock *Succ);

private:
    /// extract basic block edges to be processed
    static void
    extractBBS(const Function *func, const PostDominatorTree *postDT,
               Map<const BasicBlock *, std::vector<const BasicBlock *>> &res);

    /// extract nodes between two nodes in pdom tree
    void
    extractNodesBetweenPdomNodes(const BasicBlock *succ, const BasicBlock *LCA, const PostDominatorTree *postDT,
                                 std::vector<const BasicBlock *> &tgtNodes);

    /// dfs - extract nodes between two nodes in pdom tree
    void dfsNodesBetweenPdomNodes(const llvm::DomTreeNodeBase<BasicBlock> *cur,
                                  const llvm::DomTreeNodeBase<BasicBlock> *tgt,
                                  std::vector<const BasicBlock *> &path,
                                  std::vector<const BasicBlock *> &tgtNodes);


    /// update map
    inline void updateMap(const BasicBlock *pred, const BasicBlock *bb, s32_t pos) {
        _controlMap[pred][bb].insert(pos);
        _dependentOnMap[bb][pred].insert(pos);
    }


private:
    ControlDG *_controlDG;
    Map<const BasicBlock *, Map<const BasicBlock *, Set<s32_t>>> _controlMap; ///< map a basicblock to its controlling BBs (position, set of BBs)
    Map<const BasicBlock *, Map<const BasicBlock *, Set<s32_t>>> _dependentOnMap; ///< map a basicblock to its dependent on BBs (position, set of BBs)
    Map<const ICFGNode *, Map<const ICFGNode *, Set<s32_t>>> _nodeControlMap; ///< map an ICFG node to its controlling ICFG nodes (position, set of Nodes)
    Map<const ICFGNode *, Map<const ICFGNode *, Set<s32_t>>> _nodeDependentOnMap; ///< map an ICFG node to its dependent on ICFG nodes (position, set of Nodes)
    Map<const BasicBlock *, std::pair<const ICFGNode *, Set<const ICFGNode *>>> _bbToNode; ///< map a basic block to its inside ICFG nodes, pair first is the potential branch node

};
}


#endif //CDBUILDER_H
