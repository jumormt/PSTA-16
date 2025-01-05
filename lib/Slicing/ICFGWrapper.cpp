//
// Created by Xiao on 4/17/2022.
//

#include "Slicing/ICFGWrapper.h"

using namespace SVF;
using namespace SVFUtil;

std::unique_ptr<ICFGWrapper> ICFGWrapper::_icfgWrapper = nullptr;

void ICFGWrapper::addICFGNodeWrapperFromICFGNode(const ICFGNode *src) {

    if (!hasICFGNodeWrapper(src->getId()))
        addICFGNodeWrapper(new ICFGNodeWrapper(src));
    ICFGNodeWrapper *curICFGNodeWrapper = getGNode(src->getId());
    if (isa<FunEntryICFGNode>(src)) {
        _funcToFunEntry.emplace(src->getFun(), curICFGNodeWrapper);
    } else if (isa<FunExitICFGNode>(src)) {
        _funcToFunExit.emplace(src->getFun(), curICFGNodeWrapper);
    } else if (const CallICFGNode *callICFGNode = dyn_cast<CallICFGNode>(src)) {
        if (!hasICFGNodeWrapper(callICFGNode->getRetICFGNode()->getId()))
            addICFGNodeWrapper(new ICFGNodeWrapper(callICFGNode->getRetICFGNode()));
        if (!curICFGNodeWrapper->getRetICFGNodeWrapper())
            curICFGNodeWrapper->setRetICFGNodeWrapper(getGNode(callICFGNode->getRetICFGNode()->getId()));
    } else if (const RetICFGNode *retICFGNode = dyn_cast<RetICFGNode>(src)) {
        if (!hasICFGNodeWrapper(retICFGNode->getCallICFGNode()->getId()))
            addICFGNodeWrapper(new ICFGNodeWrapper(retICFGNode->getCallICFGNode()));
        if (!curICFGNodeWrapper->getCallICFGNodeWrapper())
            curICFGNodeWrapper->setCallICFGNodeWrapper(getGNode(retICFGNode->getCallICFGNode()->getId()));
    }
    for (const auto &e: src->getOutEdges()) {
        if (!hasICFGNodeWrapper(e->getDstID()))
            addICFGNodeWrapper(new ICFGNodeWrapper(e->getDstNode()));
        ICFGNodeWrapper *dstNodeWrapper = getGNode(e->getDstID());
        if (!hasICFGEdgeWrapper(curICFGNodeWrapper, dstNodeWrapper, e)) {
            ICFGEdgeWrapper *pEdge = new ICFGEdgeWrapper(curICFGNodeWrapper, dstNodeWrapper, e);
            addICFGEdgeWrapper(pEdge);
        }
    }
}

/*!
 * Set in N_t flag based on temporal slice
 * @param N_t
 */
void ICFGWrapper::annotateTemporalSlice(Set<u32_t> &N_t) {
    for (const auto &item: *this) {
        item.second->_inTSlice = false;
    }
    for (const auto &id: N_t) {
        if (ICFGNodeWrapper *icfgNodeWrapper = this->getICFGNodeWrapper(id))
            icfgNodeWrapper->_inTSlice = true;
    }
}

/*!
 * Set in N_t, N_s and callSites flag for reporting bug
 * @param N_t
 * @param N_s
 * @param callSites
 * @param keyNodes
 */
void ICFGWrapper::annotateMulSlice(Set<u32_t> &N_t, Set<u32_t> &N_s, Set<u32_t> &callSites, const KeyNodes &keyNodes) {
    for (const auto &item: *this) {
        item.second->_bugReport = true;
        item.second->_inTSlice = false;
        item.second->_inSSlice = false;
        item.second->_validCS = false;
        item.second->_inFSM = false;
    }
    for (const auto &id: N_t) {
        if (ICFGNodeWrapper *icfgNodeWrapper = this->getICFGNodeWrapper(id))
            icfgNodeWrapper->_inTSlice = true;
    }

    for (const auto &id: N_s) {
        if (ICFGNodeWrapper *icfgNodeWrapper = this->getICFGNodeWrapper(id))
            icfgNodeWrapper->_inSSlice = true;
    }

    u32_t nodeId = keyNodes.back();
    if (const CallICFGNode *callNode = dyn_cast<CallICFGNode>(getICFGNodeWrapper(nodeId)->getICFGNode())) {
        nodeId = callNode->getRetICFGNode()->getId();
    }
    for (const auto &e: getICFGWrapper()->getICFGNodeWrapper(nodeId)->getOutEdges()) {
        if (e->getDstNode()->getRetICFGNodeWrapper() && !e->getDstNode()->_validCS) {
            e->getDstNode()->setRetICFGNodeWrapper(nullptr);
            e->getDstNode()->_validCS = true;
        }
        e->getDstNode()->_inSSlice = true;
    }

    for (const auto &id: callSites) {
        if (ICFGNodeWrapper *icfgNodeWrapper = this->getICFGNodeWrapper(id)) {
            icfgNodeWrapper->_validCS = true;
            for (const auto &e: icfgNodeWrapper->getOutEdges()) {
                if (SVFUtil::isa<FunEntryICFGNode>(e->getDstNode()->getICFGNode())) {
                    e->getDstNode()->_inSSlice = true;
                    e->getDstNode()->_inTSlice = true;
                    ICFGNodeWrapper *nodeWrapper = const_cast<ICFGNodeWrapper *>(getFunExit(
                            e->getDstNode()->getICFGNode()->getFun()));
                    nodeWrapper->_inSSlice = true;
                    nodeWrapper->_inTSlice = true;
                }
            }
        }
    }

    for (const auto &id: keyNodes) {
        if (ICFGNodeWrapper *icfgNodeWrapper = this->getICFGNodeWrapper(id))
            icfgNodeWrapper->_inFSM = true;
    }
    getICFGNodeWrapper(keyNodes.front())->_isSrcOrSnk = true;
    getICFGNodeWrapper(keyNodes.back())->_isSrcOrSnk = true;
}

/*!
 * Annotate node on FSM
 * @param N
 */
void ICFGWrapper::annotateFSMNodes(Set<u32_t> &N) {
    for (const auto &item: *this) {
        item.second->_inFSM = false;
    }
    for (const auto &id: N) {
        if (ICFGNodeWrapper *icfgNodeWrapper = this->getICFGNodeWrapper(id))
            icfgNodeWrapper->_inFSM = true;
    }
}

/*!
 * Set in N_s flag based on spatial slice
 * @param N_s
 */
void ICFGWrapper::annotateSpatialSlice(Set<u32_t> &N_s) {
    for (const auto &item: *this) {
        item.second->_inSSlice = false;
    }
    for (const auto &id: N_s) {
        if (ICFGNodeWrapper *icfgNodeWrapper = this->getICFGNodeWrapper(id))
            icfgNodeWrapper->_inSSlice = true;
    }
}

/*!
 * Set valid callsites
 * @param N_c
 */
void ICFGWrapper::annotateCallsites(Set<u32_t> &N_c) {
    for (const auto &item: *this) {
        item.second->_validCS = false;
    }
    for (const auto &id: N_c) {
        if (ICFGNodeWrapper *icfgNodeWrapper = this->getICFGNodeWrapper(id)) {
            icfgNodeWrapper->_validCS = true;
            for (const auto &e: icfgNodeWrapper->getOutEdges()) {
                if (SVFUtil::isa<FunEntryICFGNode>(e->getDstNode()->getICFGNode())) {
                    e->getDstNode()->_inSSlice = true;
                    e->getDstNode()->_inTSlice = true;
                    ICFGNodeWrapper *nodeWrapper = const_cast<ICFGNodeWrapper *>(getFunExit(
                            e->getDstNode()->getICFGNode()->getFun()));
                    nodeWrapper->_inSSlice = true;
                    nodeWrapper->_inTSlice = true;
                }
            }
        }

    }
}

/*!
 * Remove filled color when dumping dot
 */
void ICFGWrapper::removeFilledColor() {
    for (const auto &item: *this) {
        item.second->_fillingColor = false;
        item.second->_bugReport = false;
    }
}


void ICFGWrapperBuilder::build(ICFG *icfg) {
    ICFGWrapper::releaseICFGWrapper();
    const std::unique_ptr<ICFGWrapper> &icfgWrapper = ICFGWrapper::getICFGWrapper(icfg);
    for (const auto &i: *icfg) {
        icfgWrapper->addICFGNodeWrapperFromICFGNode(i.second);
    }
}