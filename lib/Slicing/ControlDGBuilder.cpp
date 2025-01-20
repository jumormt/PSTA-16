/*
 * CDBuilder.cpp
 *
 * Ron Cytron, Jeanne Ferrante, Barry K. Rosen, Mark N. Wegman, and F. Kenneth Zadeck. 1991.
 * "Efficiently computing static single assignment form and the control dependence graph."
 * ACM Transactions on Programming Languages and Systems (TOPLAS) 13, 4 (1991), 451–490.
 * https://dl.acm.org/doi/10.1145/115372.115320
 *
 * Created by xiao on 3/21/22.
 *
 */
#include "Slicing/ControlDGBuilder.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "SVF-LLVM/LLVMUtil.h"

using namespace SVF;
using namespace SVFUtil;
using namespace llvm;

/*!
 * dfs - extract nodes between two nodes in the pdom tree
 * @param cur current processing node
 * @param tgt target node
 * @param path temp vector recording the nodes on the path
 * @param tgtNodes the result
 */
void ControlDGBuilder::dfsNodesBetweenPdomNodes(const DomTreeNodeBase<BasicBlock> *cur,
                                                const DomTreeNodeBase<BasicBlock> *tgt,
                                                std::vector<const BasicBlock *> &path,
                                                std::vector<const BasicBlock *> &tgtNodes) {
    path.push_back(cur->getBlock());
    if (cur == tgt) {
        tgtNodes.insert(tgtNodes.end(), path.begin() + 1, path.end());
    } else {
        for (DomTreeNodeBase<BasicBlock>::const_iterator pit = cur->begin(), pet = cur->end(); pit != pet; ++pit) {
            DomTreeNodeBase<BasicBlock> *nxt = *pit;
            dfsNodesBetweenPdomNodes(nxt, tgt, path, tgtNodes);
        }
    }
    path.pop_back();
}

/*!
 * (3) extract nodes from succ to the least common ancestor LCA of pred and succ
 *     including LCA if LCA is pred, excluding LCA if LCA is not pred
 * @param succ
 * @param LCA
 * @param postDT
 * @param tgtNodes
 */
void
ControlDGBuilder::extractNodesBetweenPdomNodes(const BasicBlock *succ, const BasicBlock *LCA,
                                               const PostDominatorTree *postDT,
                                               std::vector<const BasicBlock *> &tgtNodes) {
    if (succ == LCA) return;
    const DomTreeNodeBase<BasicBlock> *src = postDT->getNode(LCA);
    const DomTreeNodeBase<BasicBlock> *tgt = postDT->getNode(succ);
    std::vector<const BasicBlock *> path;
    dfsNodesBetweenPdomNodes(src, tgt, path, tgtNodes);
}

/*!
 * Start here
 */
void ControlDGBuilder::build() {
    if (_controlDG->getTotalNodeNum() > 0)
        return;
    PAG *pag = PAG::getPAG();
    buildControlDependence(pag->getModule());
    buildICFGNodeControlMap();
}

u32_t ControlDGBuilder::getBBSuccessorPos(const BasicBlock *BB, const BasicBlock *Succ) {
    u32_t i = 0;
    for (const BasicBlock *SuccBB: successors(BB))
    {
        if (SuccBB == Succ)
            return i;
        i++;
    }
    assert(false && "Didn't find succesor edge?");
    return 0;
}

/*!
 * Build control dependence for each function
 *
 * (1) construct CFG for each function
 * (2) extract basic block edges (pred->succ) on the CFG to be processed
 *     succ does not post-dominates pred (!postDT->dominates(succ, pred))
 * (3) extract nodes from succ to the least common ancestor LCA of pred and succ
 *     including LCA if LCA is pred, excluding LCA if LCA is not pred
 * @param svfgModule
 */
void ControlDGBuilder::buildControlDependence(const SVFModule *svfgModule) {
    LLVMModuleSet *llvmModuleSet = LLVMModuleSet::getLLVMModuleSet();
    for (Module &mod: llvmModuleSet->getLLVMModules()) {
        for (Module::iterator it = mod.begin(), eit = mod.end(); it != eit; ++it)
        {
            const Function* llvmFun = &*it;
            const SVFFunction* svfFun = llvmModuleSet->getSVFFunction(llvmFun);
            if (SVFUtil::isExtCall(svfFun))
                continue;
            PostDominatorTree postDT;
            postDT.recalculate(const_cast<Function &>(*llvmFun));
            // extract basic block edges to be processed
            Map<const BasicBlock *, std::vector<const BasicBlock *>> BBS;
            extractBBS(llvmFun, &postDT, BBS);
            for (const auto &item: BBS) {
                const BasicBlock *pred = item.first;
                // for each bb pair
                for (const BasicBlock *succ: item.second) {
                    const BasicBlock *LCA = postDT.findNearestCommonDominator(pred, succ);
                    std::vector<const BasicBlock *> tgtNodes;
                    if (LCA == pred) tgtNodes.push_back(LCA);
                    // from succ to LCA
                    extractNodesBetweenPdomNodes(succ, LCA, &postDT, tgtNodes);

                    s32_t pos = getBBSuccessorPos(pred, succ);
                    if (SVFUtil::isa<BranchInst>(pred->getTerminator())) {
                        pos = 1 - pos;
                    } else if (const SwitchInst *si = SVFUtil::dyn_cast<SwitchInst>(pred->getTerminator())) {
                        /// branch condition value
                        const ConstantInt *condVal = const_cast<SwitchInst *>(si)->findCaseDest(
                                const_cast<BasicBlock *>(succ));
                        /// default case is set to -1;
                        pos = condVal ? condVal->getSExtValue() : -1;
                    } else {
                        // assert(false && "not valid branch");
                        continue;
                    }
                    for (const BasicBlock *bb: tgtNodes) {
                        updateMap(pred, bb, pos);
                    }
                }
            }
        }
    }
}

/*!
 * (2) extract basic block edges on the CFG (pred->succ) to be processed
 * succ does not post-dominates pred (!postDT->dominates(succ, pred))
 * @param func
 * @param postDT
 * @return {pred: [succs]}
 */
void
ControlDGBuilder::extractBBS(const Function *func, const PostDominatorTree *postDT,
                             Map<const BasicBlock *, std::vector<const BasicBlock *>> &res) {
    for (const auto &bb: *func) {
        for (succ_const_iterator pit = succ_begin(&bb), pet = succ_end(&bb); pit != pet; ++pit) {
            const BasicBlock *succ = *pit;
            if (postDT->dominates(succ, &bb))
                continue;
            res[&bb].push_back(succ);
        }
    }
}

/*! 
 * Build map at ICFG node level
 */
void ControlDGBuilder::buildICFGNodeControlMap() {
    LLVMModuleSet *llvmModuleSet = LLVMModuleSet::getLLVMModuleSet();
    ICFG* icfg = PAG::getPAG()->getICFG();
    for (const auto &it: _controlMap) {
        for (const auto &it2: it.second) {
            const SVFBasicBlock *controllingBB = llvmModuleSet->getSVFBasicBlock(it2.first);
//            const ICFGNode *controlNode = _bbToNode[it.first].first;
//            if(!controlNode) continue;
            const Instruction *terminator = it.first->getTerminator();
            if (!terminator) continue;
            const ICFGNode *controlNode = icfg->getICFGNode(llvmModuleSet->getSVFInstruction(terminator));
            if (!controlNode) continue;
            // controlNode control at pos
            for (const auto &inst: *controllingBB) {
                const ICFGNode *controllee = icfg->getICFGNode(inst);
                _nodeControlMap[controlNode][controllee].insert(it2.second.begin(), it2.second.end());
                _nodeDependentOnMap[controllee][controlNode].insert(it2.second.begin(), it2.second.end());
                for (s32_t pos: it2.second) {
                    _controlDG->addControlDGEdgeFromSrcDst(controlNode, controllee,
                                                           SVFUtil::dyn_cast<IntraICFGNode>(controlNode)->getInst(),
                                                           pos);
                }
            }
        }
    }
}
