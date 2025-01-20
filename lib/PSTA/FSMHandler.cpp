//
// Created by Xiao on 7/21/2022.
//

#include <fstream>
#include <sstream>
#include "PSTA/config.h"
#include "Util/SVFUtil.h"
#include <sys/stat.h>

#include "PSTA/FSMHandler.h"
#include <WPA/Andersen.h>
#include "PSTA/PSAOptions.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "Util/DPItem.h"
#include "PSTA/Logger.h"
#include "SABER/SaberCondAllocator.h"

#define MAIN "main"

using namespace SVF;
using namespace SVFUtil;
using namespace std;


std::unique_ptr<FSMHandler> FSMHandler::absTransitionHandler = nullptr;
std::unique_ptr<FSMParser> FSMParser::_fsmParser = nullptr;
cJSON *FSMParser::_root = nullptr;


void FSMHandler::initMainFunc(SVFModule *module) {
    AndersenWaveDiff *ander = AndersenWaveDiff::createAndersenWaveDiff(PAG::getPAG());
    PTACallGraph *ptaCallgraph = ander->getPTACallGraph();
    // Init entry of main function
    for (const auto &func: *module) {
        // find the main function entry and save it in _mainEntry and _mainFunc
        if (func->getName() == MAIN) {
            _mainFunc = func;
            return;
        }
    }
}

void FSMHandler::initSrcs(SrcSet &srcs) {
    Log(LogLevel::Info) << "Init srcs...";
    Dump() << "Init srcs...";
    // get graphs from svf
    PAG *pag = PAG::getPAG();
    ICFG *icfg = pag->getICFG();
    AndersenWaveDiff *ander = AndersenWaveDiff::createAndersenWaveDiff(pag);
    SaberCondAllocator saberCondAllocator;
    _svfgBuilder.setSaberCondAllocator(&saberCondAllocator);
    PTACallGraph *pGraph = ander->getPTACallGraph();
    // build vfg (value flow graph)
    _svfg = _svfgBuilder.buildFullSVFG(ander);
    // TODO: add a comment
    if (PSAOptions::CxtSensitiveAlias()) buildInToOuts(_svfg);
    // get the FSM parser
    const std::unique_ptr<FSMParser> &fsmParser = FSMParser::getFSMParser();

    // get callsiteRets, e.g., a = fun(), a is the ret
    OrderedSet<const RetICFGNode *> css;
    for (PAG::CSToRetMap::iterator it = pag->getCallSiteRets().begin(),
                 eit = pag->getCallSiteRets().end(); it != eit; ++it) {
        css.insert(it->first);
    }
    // TODO: add a comment
    OrderedMap<const ICFGNode *, const SVFGNode *> tmp;
    for (const auto &cs: css) {
        /// if this callsite return reside in a dead function then we do not care about its leaks
        /// for example instruction `int* p = malloc(size)` is in a dead function, then program won't allocate this memory
        /// for example a customized malloc `int p = malloc()` returns an integer value, then program treat it _typeState a system malloc
        if (cs->getCallSite()->ptrInUncalledFunction() || !cs->getCallSite()->getType()->isPointerTy())
            continue;

        // get all callees
        PTACallGraph::FunctionSet callees;
        pGraph->getCallees(cs->getCallICFGNode(), callees);
        OrderedSet<const SVFFunction *> orderedCallees(callees.begin(), callees.end());

        // find the callee we are interested in, e.g., malloc
        for (const auto &fun: orderedCallees) {
            if (fsmParser->getTypeFromStr(fun->getName()) == fsmParser->getSrcAction()) {
                FIFOWorkList<const CallICFGNode *> worklist;
                NodeBS visited;
                worklist.push(cs->getCallICFGNode());
                while (!worklist.empty()) {
                    const CallICFGNode *callBlockNode = worklist.pop();
                    const RetICFGNode *retBlockNode = icfg->getRetICFGNode(callBlockNode->getCallSite());
                    const PAGNode *pagNode = pag->getCallSiteRet(retBlockNode);
                    const SVFGNode *node = _svfg->getDefSVFGNode(pagNode);
                    if (visited.test(node->getId()) == 0)
                        visited.set(node->getId());
                    else
                        continue;

                    CallSiteSet csSet;
                    // if this node is in an allocation wrapper, find all its call nodes
                    if (isInAWrapper(node, csSet, _svfg, pGraph)) {
                        for (const auto &callee: csSet) {
                            worklist.push(callee);
                        }
                    }
                        // otherwise, this is the source we are interested
                    else {
                        // exclude sources in dead functions
                        if (!callBlockNode->getCallSite()->ptrInUncalledFunction()) {
                            tmp[node->getICFGNode()] = node;
                        }
                    }
                }
            }
        }
    }
    for (const auto &src: tmp) {
        if (srcs.size() < PSAOptions::MaxSrcLimit()) srcs.insert(src.second);
        else break;
    }
    Log(LogLevel::Info) << SVFUtil::sucMsg("[done]\n");
    Dump() << SVFUtil::sucMsg("[done]\n");
    Log(LogLevel::Info) << "Src size: " << std::to_string(srcs.size()) << "\n";
    Dump() << "Src size: " << std::to_string(srcs.size()) << "\n";
}


/*!
 * Determine whether a SVFGNode n is in a allocation wrapper function,
 * if so, return all SVFGNodes which receive the value of node n
 */
bool
FSMHandler::isInAWrapper(const SVFGNode *src, CallSiteSet &csIdSet, SVFG *svfg, PTACallGraph *ptaCallGraph) {

    bool reachFunExit = false;
    const std::unique_ptr<FSMParser> &fsmParser = FSMParser::getFSMParser();


    FIFOWorkList<const SVFGNode *> worklist;
    worklist.push(src);
    NodeBS visited;
    u32_t step = 0;
    while (!worklist.empty()) {
        const SVFGNode *node = worklist.pop();

        if (visited.test(node->getId()) == 0)
            visited.set(node->getId());
        else
            continue;
        // reaching maximum steps when traversing on SVFG to identify a memory allocation wrapper
        if (step++ > PSAOptions::MaxStepInWrapper())
            return false;

        for (SVFGNode::const_iterator it = node->OutEdgeBegin(), eit =
                node->OutEdgeEnd(); it != eit; ++it) {
            const SVFGEdge *edge = (*it);
            //assert(edge->isDirectVFGEdge() && "the edge should always be direct VF");
            // if this is a call edge
            if (edge->isCallDirectVFGEdge()) {
                return false;
            }
                // if this is a return edge
            else if (edge->isRetDirectVFGEdge()) {
                reachFunExit = true;
                csIdSet.insert(svfg->getCallSite(SVFUtil::cast<RetDirSVFGEdge>(edge)->getCallSiteId()));
            }
                // (1) an intra direct edge, we will keep tracking
                // (2) an intra indirect edge, we only track if the succ SVFGNode is a load, which means we only track one level store-load pair .
                // (3) do not track for all other interprocedural edges.
                // (4) terminate when meeting non-src action
            else {
                const SVFGNode *succ = edge->getDstNode();
                // terminate when meeting non-src action
                if (const CallICFGNode *callBlockNode = dyn_cast<CallICFGNode>(succ->getICFGNode())) {
                    Set<const SVFFunction *> functionSet;
                    ptaCallGraph->getCallees(callBlockNode, functionSet);
                    if (!functionSet.empty()) {
                        FSMParser::FSMAction action = getTypeFromFunc((*functionSet.begin()));
                        // for non-src actions
                        if (fsmParser->getFSMActions().count(action) && action != fsmParser->getSrcAction() &&
                            action != FSMParser::CK_DUMMY)
                            return false;
                    }
                }
                if (SVFUtil::isa<IntraDirSVFGEdge>(edge)) {
                    if (SVFUtil::isa<CopySVFGNode>(succ) || SVFUtil::isa<GepSVFGNode>(succ)
                        || SVFUtil::isa<PHISVFGNode>(succ) || SVFUtil::isa<FormalRetSVFGNode>(succ)
                        || SVFUtil::isa<ActualRetSVFGNode>(succ) || SVFUtil::isa<StoreSVFGNode>(succ)) {
                        worklist.push(succ);
                    }
                } else if (SVFUtil::isa<IntraIndSVFGEdge>(edge)) {
                    if (SVFUtil::isa<LoadSVFGNode>(succ)) {
                        worklist.push(succ);
                    }
                } else
                    return false;
            }
        }
    }
    if (reachFunExit)
        return true;
    else
        return false;
}

/*!
 * Abstract state transition
 */
const TypeState &
FSMHandler::absStateTransition(const TypeState &curAbsState, const FSMParser::FSMAction &action) {
    const FSMParser::FSM &fsm = FSMParser::getFSMParser()->getFSM();
    auto it = fsm.find(curAbsState);
    assert(it != fsm.end() && "Abstract state not in FSM!");
    auto it2 = it->second.find(action);
    assert(it2 != it->second.end() && "Action not in FSM!");
    return it2->second;
}

/*!
 * Annotate ICFG with abstract state transfer function,
 * so that we do not need to care about FSM when doing AS transition
 */
void FSMHandler::initAbsTransitionFuncs(const SVFGNode *src, Set<const SVFFunction *> &curEvalFuns,
                                        bool recordGlobal) {
    clearICFGAbsTransferMap();
    Log(LogLevel::Info) << "Init abs transition funcs...";
    Dump() << "Init abs transition funcs...";
    const std::unique_ptr<FSMParser> &fsmParser = FSMParser::getFSMParser();
    PTACallGraph *ptaCallGraph = AndersenWaveDiff::createAndersenWaveDiff(PAG::getPAG())->getPTACallGraph();
    const ICFGNode *srcICFGNode = src->getICFGNode();
    Set<CxtDPItem> workListLayer;
    ContextCond::setMaxCxtLen(PSAOptions::CxtLimit());
    Set<u32_t> visited;
    ContextCond cxt;
    CxtDPItem dpItem(src->getId(), cxt);
    visited.insert(src->getId());
    workListLayer.insert(SVFUtil::move(dpItem));
    while (!workListLayer.empty()) {
        Set<CxtDPItem> nxtWorkListLayer;
        for (const auto &item: workListLayer) {
            SVFGNode *svfgNode = _svfg->getSVFGNode(item.getCurNodeID());
            bool isSrc = false;
            if (!SVFUtil::isa<MSSAPHISVFGNode>(svfgNode)) {
                if (const RetICFGNode *retBlockNode = dyn_cast<RetICFGNode>(svfgNode->getICFGNode())) {
                    // if node is in wrapper, srcICFGNode is a RetICFGNode
                    if (retBlockNode == srcICFGNode) {
                        isSrc = true;
                        // for src action
                        for (const auto &srcAbsState: fsmParser->getAbsStates()) {
                            const TypeState &dstAbsState = absStateTransition(srcAbsState,
                                                                              fsmParser->getSrcAction());
                            // For APIs transition func is set on Callsites
                            icfgAbsTransitionFunc[retBlockNode->getCallICFGNode()].emplace(srcAbsState, dstAbsState);
                        }
                    }
                }
                if (const CallICFGNode *callBlockNode = dyn_cast<CallICFGNode>(svfgNode->getICFGNode())) {
                    if (callBlockNode == srcICFGNode) {
                        isSrc = true;
                        // for src action
                        for (const auto &srcAbsState: fsmParser->getAbsStates()) {
                            const TypeState &dstAbsState = absStateTransition(srcAbsState,
                                                                              fsmParser->getSrcAction());
                            // For APIs transition func is set on Callsites
                            icfgAbsTransitionFunc[callBlockNode].emplace(srcAbsState, dstAbsState);
                        }
                    } else {
                        Set<const SVFFunction *> functionSet;
                        ptaCallGraph->getCallees(callBlockNode, functionSet);
                        if (functionSet.empty())
                            continue;
                        FSMParser::FSMAction action = getTypeFromFunc((*functionSet.begin()));
                        // for non-src actions
                        if (fsmParser->getFSMActions().count(action) && action != fsmParser->getSrcAction()) {
                            for (const auto &srcAbsState: fsmParser->getAbsStates()) {
                                const TypeState &dstAbsState = absStateTransition(srcAbsState, action);
                                icfgAbsTransitionFunc[callBlockNode].emplace(srcAbsState, dstAbsState);
                            }
                        }
                    }
                } else if (const StmtVFGNode *stmtVFGNode = dyn_cast<StmtVFGNode>(svfgNode)) {
//                 if no wrapper, we should identify load as use node
                    if (!PSAOptions::Wrapper() && PSAOptions::LoadAsUse()) {
                        if (isa<LoadStmt>(stmtVFGNode->getPAGEdge())) {
                            if (fsmParser->getFSMActions().count(FSMParser::CK_USE)) {
                                for (const auto &srcAbsState: fsmParser->getAbsStates()) {
                                    const TypeState &dstAbsState = absStateTransition(srcAbsState,
                                                                                      FSMParser::CK_USE);
                                    icfgAbsTransitionFunc[svfgNode->getICFGNode()].emplace(srcAbsState, dstAbsState);
                                }
                            }
                        }
                    }
                } else {
                    DBOUT(DGENERAL, outs() << pasMsg("non-transition nodes\n"));
                }
            }
            for (const auto &edge: svfgNode->getOutEdges()) {
                // for indirect SVFGEdge, the propagation should follow the def-use chains
                // points-to on the edge indicate whether the object of source node can be propagated

                const SVFGNode *dstNode = edge->getDstNode();
                CxtDPItem newItem(dstNode->getId(), item.getContexts());

                /// handle globals here
                if (recordGlobal)
                    if (reachGlobal(src) || _svfgBuilder.isGlobalSVFGNode(dstNode)) {
                        reachGlobalNodes.insert(src);
                        continue;
                    }

                /// perform context sensitive reachability
                // push context for calling
                if (edge->isCallVFGEdge()) {
                    FSMParser::FSMAction action = getTypeFromFunc(
                            edge->getDstNode()->getFun());
                    if (curEvalFuns.count(edge->getDstNode()->getFun()) || isSrc ||
                        (fsmParser->getFSMActions().count(action) && action != FSMParser::CK_DUMMY))
                        continue;
                    CallSiteID csId = 0;
                    if (const CallDirSVFGEdge *callEdge = SVFUtil::dyn_cast<CallDirSVFGEdge>(edge))
                        csId = callEdge->getCallSiteId();
                    else
                        csId = SVFUtil::cast<CallIndSVFGEdge>(edge)->getCallSiteId();

                    newItem.pushContext(csId);
                }
                    // match context for return
                else if (edge->isRetVFGEdge()) {
                    FSMParser::FSMAction action = getTypeFromFunc(
                            edge->getSrcNode()->getFun());
                    if (curEvalFuns.count(edge->getSrcNode()->getFun()) || isSrc ||
                        (fsmParser->getFSMActions().count(action) && action != FSMParser::CK_DUMMY))
                        continue;
                    CallSiteID csId = 0;
                    if (const RetDirSVFGEdge *callEdge = SVFUtil::dyn_cast<RetDirSVFGEdge>(edge))
                        csId = callEdge->getCallSiteId();
                    else
                        csId = SVFUtil::cast<RetIndSVFGEdge>(edge)->getCallSiteId();

                    if (newItem.matchContext(csId) == false) continue;
                }

                /// whether this dstNode has been visited or not
                if (visited.count(newItem.getCurNodeID())) continue;
                else visited.insert(newItem.getCurNodeID());
                nxtWorkListLayer.insert(SVFUtil::move(newItem));
            }

            // Jump from actual in to its reachable actual outs
            if (SVFUtil::isa<ActualINSVFGNode>(svfgNode) || SVFUtil::isa<ActualParmVFGNode>(svfgNode)) {
                for (const auto &actualout: getReachableActualOutsOfActualIn(
                        svfgNode)) {
                    if (recordGlobal)
                        if (reachGlobal(src) || _svfgBuilder.isGlobalSVFGNode(actualout)) {
                            reachGlobalNodes.insert(src);
                            continue;
                        }
                    CxtDPItem newItem(actualout->getId(), item.getContexts());
                    if (visited.count(newItem.getCurNodeID())) continue;
                    else visited.insert(newItem.getCurNodeID());
                    nxtWorkListLayer.insert(SVFUtil::move(newItem));
                }
            }
        }
        workListLayer = SVFUtil::move(nxtWorkListLayer);
    }

    for (const auto &item: *(PAG::getPAG()->getICFG())) {
        const ICFGNode *icfgNode = item.second;
        if (fsmParser->getFSMActions().count(FSMParser::CK_RET)) {
            if (const IntraICFGNode *intraBlockNode = dyn_cast<IntraICFGNode>(icfgNode)) {
                if (intraBlockNode->getFun()->getName() == MAIN && intraBlockNode->getInst()->isRetInst()) {
                    for (const auto &srcAbsState: fsmParser->getAbsStates()) {
                        const TypeState &dstAbsState = absStateTransition(srcAbsState, FSMParser::CK_RET);
                        icfgAbsTransitionFunc[icfgNode].emplace(srcAbsState, dstAbsState);
                    }
                }
            }
        }
    }
    Log(LogLevel::Info) << SVFUtil::sucMsg("[done]\n");
    Dump() << SVFUtil::sucMsg("[done]\n");
}

/*!
 * Init object to ICFG sinks map, e.g., UAFFunc
 */
void
FSMHandler::initSnks(const SVFGNode *src, ICFGNodeSet &snks, Set<FSMParser::CHECKER_TYPE> &checkerTypes, bool noLimit) {
    snks.clear();
    Log(LogLevel::Info) << "Init snk points...";
    Dump() << "Init snk points...";
    PAG *pag = PAG::getPAG();
    AndersenWaveDiff *ander = AndersenWaveDiff::createAndersenWaveDiff(pag);
    PTACallGraph *ptaCallGraph = ander->getPTACallGraph();
    const std::unique_ptr<FSMParser> &fsmParser = FSMParser::getFSMParser();

    for (const auto &mpItem: icfgAbsTransitionFunc) {
        if (const CallICFGNode *callBlockNode = dyn_cast<CallICFGNode>(mpItem.first)) {
            Set<const SVFFunction *> functionSet;
            ptaCallGraph->getCallees(callBlockNode, functionSet);
            if (functionSet.empty())
                continue;
            FSMParser::FSMAction action = getTypeFromFunc((*functionSet.begin()));
            // for non-src actions
            if ((noLimit || PSAOptions::MaxSnkLimit() == 0 || snks.size() < PSAOptions::MaxSnkLimit()) &&
                checkerTypes.find(action) != checkerTypes.end()) {
                snks.insert(callBlockNode);
            }
        } else if (const IntraICFGNode *intraICFGNode = dyn_cast<IntraICFGNode>(mpItem.first)) {
            if (PSAOptions::LoadAsUse() &&
                (noLimit || PSAOptions::MaxSnkLimit() == 0 || snks.size() < PSAOptions::MaxSnkLimit()) && !PSAOptions::Wrapper() &&
                checkerTypes.find(FSMParser::CK_USE) != checkerTypes.end()) {
                std::list<const SVFStmt *> svfStmts = intraICFGNode->getSVFStmts();
                if (!svfStmts.empty()) {
                    for (const auto &svfStmt: svfStmts) {
                        if (isa<LoadStmt>(svfStmt)) {
                            snks.insert(mpItem.first);
                        }
                    }
                }
            }
            if (checkerTypes.find(FSMParser::CK_RET) != checkerTypes.end()) {
                if (intraICFGNode->getFun()->getName() == MAIN && intraICFGNode->getInst()->isRetInst()) {
                    snks.insert(mpItem.first);
                }
            }
        } else {
            DBOUT(DGENERAL, outs() << pasMsg("no snkMap\n"));
        }
    }
    Log(LogLevel::Info) << SVFUtil::sucMsg("[done]\n");
    Dump() << SVFUtil::sucMsg("[done]\n");
}


void FSMHandler::buildOutToIns(SVFG *svfg) {
    Set<const SVFGNode *> visitedOuts;
    for (const auto &it: *svfg) {
        if (SVFUtil::isa<ActualOUTSVFGNode>(it.second) || SVFUtil::isa<ActualRetVFGNode>(it.second)) {
            computeOutToIns(it.second, visitedOuts);
        }
    }
}

void FSMHandler::buildInToOuts(SVFG *svfg) {
    Set<const SVFGNode *> visitedOuts;
    for (const auto &it: *svfg) {
        if (SVFUtil::isa<ActualINSVFGNode>(it.second) || SVFUtil::isa<ActualParmVFGNode>(it.second)) {
            computeInToOuts(it.second, visitedOuts);
        }
    }
}

void FSMHandler::computeOutToIns(const SVFGNode *src, Set<const SVFGNode *> &visitedOuts) {
    if (_outToIns.count(src) || visitedOuts.count(src)) return;
    visitedOuts.insert(src);

    FIFOWorkList<const SVFGNode *> workList;
    Set<const SVFGNode *> visited;

    u32_t callSiteID;
    Set<const SVFGNode *> ins;
    for (const auto &inEdge: src->getInEdges()) {
        workList.push(inEdge->getSrcNode());
        visited.insert(inEdge->getSrcNode());
        if (const RetDirSVFGEdge *retEdge = SVFUtil::dyn_cast<RetDirSVFGEdge>(inEdge)) {
            callSiteID = retEdge->getCallSiteId();
        } else if (const RetIndSVFGEdge *retEdge = SVFUtil::dyn_cast<RetIndSVFGEdge>(inEdge)) {
            callSiteID = retEdge->getCallSiteId();
        } else {
            assert(false && "return edge does not have a callsite ID?");
        }
    }

    while (!workList.empty()) {
        const SVFGNode *cur = workList.pop();
        if (SVFUtil::isa<FormalINSVFGNode>(cur) || SVFUtil::isa<FormalParmVFGNode>(cur)) {
            for (const auto &inEdge: cur->getInEdges()) {
                if (const CallDirSVFGEdge *callEdge = SVFUtil::dyn_cast<CallDirSVFGEdge>(inEdge)) {
                    if (callSiteID == callEdge->getCallSiteId()) ins.insert(callEdge->getSrcNode());
                } else if (const CallIndSVFGEdge *callEdge = SVFUtil::dyn_cast<CallIndSVFGEdge>(inEdge)) {
                    if (callSiteID == callEdge->getCallSiteId()) ins.insert(callEdge->getSrcNode());
                } else {
                    assert(false && "call edge does not have a callsite ID?");
                }
            }
            continue;
        }
        if (SVFUtil::isa<ActualOUTSVFGNode>(cur) || SVFUtil::isa<ActualRetVFGNode>(cur)) {
            computeOutToIns(cur, visitedOuts);
            for (const auto &in: _outToIns[cur]) {
                if (!visited.count(in)) {
                    visited.insert(in);
                    workList.push(in);
                }
            }
            continue;
        }
        for (const auto &inEdge: cur->getInEdges()) {
            if (inEdge->getSrcNode()->getFun() == cur->getFun() && !visited.count(inEdge->getSrcNode())) {
                visited.insert(inEdge->getSrcNode());
                workList.push(inEdge->getSrcNode());
            }
        }
    }
    _outToIns[src] = ins;
}

void FSMHandler::computeInToOuts(const SVFGNode *src, Set<const SVFGNode *> &visitedIns) {
    if (_inToOuts.count(src) || visitedIns.count(src)) return;
    visitedIns.insert(src);

    FIFOWorkList<const SVFGNode *> workList;
    Set<const SVFGNode *> visited;

    u32_t callSiteID;
    Set<const SVFGNode *> ins;
    for (const auto &outEdge: src->getOutEdges()) {
        workList.push(outEdge->getDstNode());
        visited.insert(outEdge->getDstNode());
        if (const CallDirSVFGEdge *callEdge = SVFUtil::dyn_cast<CallDirSVFGEdge>(outEdge)) {
            callSiteID = callEdge->getCallSiteId();
        } else if (const CallIndSVFGEdge *callEdge = SVFUtil::dyn_cast<CallIndSVFGEdge>(outEdge)) {
            callSiteID = callEdge->getCallSiteId();
        } else {
            assert(false && "return edge does not have a callsite ID?");
        }
    }

    while (!workList.empty()) {
        const SVFGNode *cur = workList.pop();
        if (SVFUtil::isa<FormalOUTSVFGNode>(cur) || SVFUtil::isa<FormalRetVFGNode>(cur)) {
            for (const auto &outEdge: cur->getOutEdges()) {
                if (const RetDirSVFGEdge *retEdge = SVFUtil::dyn_cast<RetDirSVFGEdge>(outEdge)) {
                    if (callSiteID == retEdge->getCallSiteId()) ins.insert(retEdge->getDstNode());
                } else if (const RetIndSVFGEdge *retEdge = SVFUtil::dyn_cast<RetIndSVFGEdge>(outEdge)) {
                    if (callSiteID == retEdge->getCallSiteId()) ins.insert(retEdge->getDstNode());
                } else {
                    assert(false && "return edge does not have a callsite ID?");
                }
            }
            continue;
        }
        if (SVFUtil::isa<ActualINSVFGNode>(cur) || SVFUtil::isa<ActualParmVFGNode>(cur)) {
            computeInToOuts(cur, visitedIns);
            for (const auto &in: _inToOuts[cur]) {
                if (!visited.count(in)) {
                    visited.insert(in);
                    workList.push(in);
                }
            }
            continue;
        }
        for (const auto &outEdge: cur->getOutEdges()) {
            if (outEdge->getDstNode()->getFun() == cur->getFun() && !visited.count(outEdge->getDstNode())) {
                visited.insert(outEdge->getDstNode());
                workList.push(outEdge->getDstNode());
            }
        }
    }
    _inToOuts[src] = ins;
}


static cJSON *parseJson(const std::string &path, off_t fileSize) {
    FILE *file = fopen(path.c_str(), "r");
    if (!file) {
        return nullptr;
    }

    // allocate memory size matched with file size
    char *jsonStr = (char *) calloc(fileSize + 1, sizeof(char));

    // read json string from file
    u32_t size = fread(jsonStr, sizeof(char), fileSize, file);
    if (size == 0) {
        SVFUtil::errs() << SVFUtil::errMsg("\t Wrong ExtAPI.json path!! ") << "The current ExtAPI.json path is: "
                        << path << "\n";
        assert(false && "Read ExtAPI.json file fails!");
        return nullptr;
    }
    fclose(file);

    // convert json string to json pointer variable
    cJSON *root = cJSON_Parse(jsonStr);
    if (!root) {
        free(jsonStr);
        return nullptr;
    }
    free(jsonStr);
    return root;
}

FSMParser::FSMParser(const std::string &fsmFile) {
    std::string jsonPath = PROJECT_SOURCE_ROOT;
    jsonPath += CHECKERAPI_JSON_PATH;
    struct stat statbuf{};
    if (!stat(jsonPath.c_str(), &statbuf)) {
        _root = parseJson(jsonPath, statbuf.st_size);
    }
    buildFSM(fsmFile);
}

// Get specifications of external functions in ExtAPI.json file
cJSON *FSMParser::get_FunJson(const std::string &funName) const {
    assert(_root && "JSON not loaded");
    return cJSON_GetObjectItemCaseSensitive(_root, funName.c_str());
}

FSMParser::FSMAction FSMParser::get_type(const std::string &funName) const {
    cJSON *item = get_FunJson(funName);
    std::string type;
    if (item != nullptr) {
        //  Get the first operation of the function
        cJSON *obj = item->child;
        if (strcmp(obj->string, "type") == 0)
            type = obj->valuestring;
        else
            assert(false && "The function operation format is illegal!");
    }
    auto it = _typeMap.find(type);
    if (it == _typeMap.end())
        return CK_DUMMY;
    else
        return it->second;
}

// Get property of the operation, e.g. "EFT_A1R_A0R"
FSMParser::FSMAction FSMParser::get_type(const SVF::SVFFunction *F) {
    return get_type(get_name(F));
}

// Get external function name, e.g "memcpy"
std::string FSMParser::get_name(const SVFFunction *F) {
    assert(F);
    std::string funName = F->getName();
    if (F->isIntrinsic()) {
        unsigned start = funName.find('.');
        unsigned end = funName.substr(start + 1).find('.');
        funName = "llvm." + funName.substr(start + 1, end);
    }
    return funName;
}

/*!
 * build finite state machine for abstract state transition
 */
void FSMParser::buildFSM(const std::string &_fsmFile) {
    clearFSM();
    string line;
    ifstream fsmFile(_fsmFile.c_str());
    assert(fsmFile.good() && "fsm file not exists!");
    if (fsmFile.is_open()) {
        u32_t lineNum = 0;
        while (fsmFile.good()) {
            getline(fsmFile, line);
            vector<string> tks;
            istringstream ss(line);
            while (ss.good()) {
                string tk;
                ss >> tk;
                if (!tk.empty())
                    tks.push_back(tk);
            }
            if (lineNum == 0) {
                // abstract state domain
                for (const auto &tk: tks) {
                    _typestates.insert(TypeStateParser::fromString(tk));
                }
                uninit = TypeStateParser::fromString(tks[0]);
                _err = TypeStateParser::fromString(tks[1]);
            } else if (lineNum == 1) {
                // action domain
                _srcAction = getTypeFromStr(tks[0]);
                for (const auto &tk: tks) {
                    _fsmActions.insert(getTypeFromStr(tk));
                }
            } else {
                // state transition
                if (tks[1] != "*")
                    _fsm[TypeStateParser::fromString(tks[0])][getTypeFromStr(tks[1])] = TypeStateParser::fromString(
                            tks[2]);
                else {
                    for (const auto &action: _fsmActions) {
                        _fsm[TypeStateParser::fromString(tks[0])][action] = TypeStateParser::fromString(tks[2]);
                    }
                }
                // do no change state for CK_DUMMY
                _fsm[TypeStateParser::fromString(tks[0])].emplace(CK_DUMMY, TypeStateParser::fromString(tks[0]));
            }
            ++lineNum;
        }
    }
    assert(_err != TypeState::Unknown && uninit != TypeState::Unknown && "no specified error state!");

}

