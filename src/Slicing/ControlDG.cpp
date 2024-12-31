/*
 * ControlDG.cpp
 *
 * Control Dependence Graph, Node contains ICFG Node, Edge is labeled with branch condition
 *
 * Created by xiao on 3/21/22.
 *
 */
#include "ControlDG.h"

using namespace SVF;

ControlDG *ControlDG::controlDg = nullptr;

void ControlDG::addControlDGEdgeFromSrcDst(const ICFGNode *src, const ICFGNode *dst, const SVFValue *pNode, s32_t branchID) {
    if (!hasControlDGNode(src->getId())) {
        addGNode(src->getId(), new ControlDGNode(src));
    }
    if (!hasControlDGNode(dst->getId())) {
        addGNode(dst->getId(), new ControlDGNode(dst));
    }
    if (!hasControlDGEdge(getControlDGNode(src->getId()), getControlDGNode(dst->getId()))) {
        ControlDGEdge *pEdge = new ControlDGEdge(getControlDGNode(src->getId()),
                                                 getControlDGNode(dst->getId()));
        pEdge->insertBranchCondition(pNode, branchID);
        addControlDGEdge(pEdge);
        incEdgeNum();
    } else {
        ControlDGEdge *pEdge = getControlDGEdge(getControlDGNode(src->getId()),
                                                getControlDGNode(dst->getId()));
        pEdge->insertBranchCondition(pNode, branchID);
    }
}