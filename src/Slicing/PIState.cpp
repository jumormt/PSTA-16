//
// Created by Xiao on 2022/4/4.
//

#include "Slicing/PIState.h"
#include "WPA/Andersen.h"
#include "SABER/SaberSVFGBuilder.h"

using namespace SVF;
using namespace std;
using namespace SVFUtil;


PIStateManager::~PIStateManager() {
    releasePIStates();
}

void PIStateManager::releasePIStates() {
    for (const auto &it: _hashToPIStateMap) {
        delete it.second;
    }
}

/*!
 * Get or add a single sequence state
 */
const PIState *
PIStateManager::getOrAddPIState(PI sqs, TypeState absState) {
    size_t ha = computeHashOfPIState(sqs, absState);
    auto it = _hashToPIStateMap.find(ha);
    if (it != _hashToPIStateMap.end())
        return it->second;
    const auto *sqState = new PIState(SVFUtil::move(absState), SVFUtil::move(sqs));
    _hashToPIStateMap.emplace(ha, sqState);
    return sqState;
}

void PIStateManager::nonBranchFlowFun(const ICFGNode *icfgNode, PIState *sqState, const SVFGNode *svfgNode) {
    ICFG *icfg = PAG::getPAG()->getICFG();
    auto it2 = getICFGAbsTransferMap().find(icfgNode);
    if (it2 != getICFGAbsTransferMap().end()) {
        // have abstract state transition
        PI sqs_n;
        NodeID nodeId = icfgNode->getId();
        if (const CallICFGNode *callIcfgNode = dyn_cast<CallICFGNode>(icfgNode)) {
            // nodeId is the return node of icfgNode
            nodeId = callIcfgNode->getRetICFGNode()->getId();
        }
        // _curEvalICFGNode is callnode
        if (icfgNode == _curEvalICFGNode) {
            // make sure alloc for once
            if (sqState->getPI().empty() &&
                sqState->getAbstractState() == FSMParser::getFSMParser()->getUninitAbsState()) {
                // for API transition node, nodeId is the return node of icfgNode
                // we put retnode id in the sequence
                sqs_n.insert(DataFact{nodeId});
                const TypeState &nxtAbsState = it2->second.find(sqState->getAbstractState())->second;
                sqState->_absState = nxtAbsState;
                sqState->_PI = std::move(sqs_n);
                return;
            } else {
                return;
            }
        } else {
            FSMParser::FSMAction curAction = getActionOfICFGNode(icfgNode);
            // not interested action
            if (FSMParser::getFSMParser()->getFSMActions().find(curAction) ==
                FSMParser::getFSMParser()->getFSMActions().end() ||
                curAction == FSMParser::CK_DUMMY)
                return;
//            if((_snkTypes.find(curAction) != _snkTypes.end() && _curSnks.find(icfgNode) == _curSnks.end()))
//                return sqState;
            const TypeState &predAbsState = sqState->getAbstractState();
            const TypeState &nxtAbsState = it2->second.find(predAbsState)->second;
            if (predAbsState == nxtAbsState) {
                bool changed = false;
                for (const auto &seq: sqState->getPI()) {
                    ICFGNode *preNode = icfg->getICFGNode(seq.back());
                    FSMParser::FSMAction preAction = getActionOfICFGNode(preNode);
                    sqs_n.insert(seq);
                    if (curAction == preAction) {
                        changed = true;
                        DataFact df = seq;
                        df.pop_back();
                        df.push_back(nodeId);
                        sqs_n.insert(df);
                    }
                }
                if (changed) {
                    sqState->_absState = nxtAbsState;
                    sqState->_PI = std::move(sqs_n);
                    return;
                }
                else
                    return;
            } else {
                // if FSM state changed, we add current nodeid to each of the PI
                // for API transition node, nodeId is the return node of icfgNode
                // we put retnode id in the sequence
                if (sqState->getPI().empty()) {
                    sqs_n.insert(DataFact{nodeId});
                } else {
                    for (const auto &sq: sqState->getPI()) {
                        DataFact df = sq;
                        df.push_back(nodeId);
                        sqs_n.insert(SVFUtil::move(df));
                    }
                }
            }
            sqState->_absState = nxtAbsState;
            sqState->_PI = std::move(sqs_n);
            return;
        }
    } else {
        // no abstract state transition
        return;
    }
}

FSMParser::FSMAction
PIStateManager::getActionOfICFGNode(const ICFGNode *icfgNode) {
    if (const CallICFGNode *callBlockNode = dyn_cast<CallICFGNode>(icfgNode)) {
        Set<const SVFFunction *> functionSet;
        getPTACallGraph()->getCallees(callBlockNode, functionSet);
        if (functionSet.empty())
            return FSMParser::CK_DUMMY;
        if (getICFGAbsTransferMap().count(icfgNode)) {
            return FSMHandler::getAbsTransitionHandler()->getTypeFromFunc((*functionSet.begin()));
        }
        return FSMParser::getFSMParser()->getTypeFromStr((*functionSet.begin())->getName());
    } else if (const RetICFGNode *retNode = dyn_cast<RetICFGNode>(icfgNode)) {
        const CallICFGNode *callBlockNode = retNode->getCallICFGNode();
        Set<const SVFFunction *> functionSet;
        getPTACallGraph()->getCallees(callBlockNode, functionSet);
        if (functionSet.empty())
            return FSMParser::CK_DUMMY;
        if (getICFGAbsTransferMap().count(callBlockNode)) {
            return FSMHandler::getAbsTransitionHandler()->getTypeFromFunc((*functionSet.begin()));
        }
        return FSMParser::getFSMParser()->getTypeFromStr((*functionSet.begin())->getName());
    } else if (const IntraICFGNode *intraICFGNode = dyn_cast<IntraICFGNode>(icfgNode)) {
        std::list<const SVFStmt *> svfStmts = intraICFGNode->getSVFStmts();
        if (!svfStmts.empty() && PSAOptions::LoadAsUse()) {
            for (const auto &svfStmt: svfStmts) {
                if (isa<LoadStmt>(svfStmt)) {
                    return FSMParser::CK_USE;
                }
            }
        }
        if (intraICFGNode->getFun() == getFSMHandler()->getMainFunc() && intraICFGNode->getInst()->isRetInst()) {
            return FSMParser::CK_RET;
        }
        return FSMParser::CK_DUMMY;
    } else {
        return FSMParser::CK_DUMMY;
    }
}

size_t PIStateManager::computeHashOfPIState(const PI &sqs, const TypeState &absState) {
    size_t h = sqs.size();
    SVF::Hash<PIState::DataFact> hf;
    for (const auto &t: sqs) {
        h ^= hf(t) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    SVF::Hash<std::pair<size_t, int>> pairH;

    return pairH(std::make_pair(h, static_cast<int>(absState)));
}