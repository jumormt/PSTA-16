//
// Created by Xiao on 7/24/2022.
//

#include "Slicing/TemporalSlicer.h"
#include "PSTA/PSAOptions.h"
#include "PSTA/Logger.h"

using namespace SVF;
using namespace SVFUtil;

/*!
 * Temporal slicing
 * @param srcs
 * @param mainEntry
 */
void TemporalSlicer::temporalSlicing(const SVFGNode *src, const ICFGNodeWrapper *mainEntry) {
    Log(LogLevel::Info) << "Temporal slicing...\n";
    Dump() << "Temporal slicing...\n";
    Log(LogLevel::Info) << " TPS src: " << std::to_string(src->getICFGNode()->getId()) << "\n";
    Dump() << " TPS src: " << std::to_string(src->getICFGNode()->getId()) << "\n";
    Set<u32_t> temporalSlice;
    Log(LogLevel::Info) << "seqs size: " << std::to_string(_sQ.size()) << "\n";
    Dump() << "seqs size: " << std::to_string(_sQ.size()) << "\n";

    u32_t ct2 = 0;
    for (const auto &dataFact: _sQ) {
        ct2++;
        clearDF();
        std::vector<DataFact> allDataFacts;
        DataFact dataFactTmp = dataFact;
        Set<u32_t> temporalSliceTmp;
        while (!dataFactTmp.empty()) {
            allDataFacts.push_back(dataFactTmp);
            dataFactTmp.pop_back();
        }
        // zero data fact
        allDataFacts.emplace_back();
        initBUDFTransferFunc(mainEntry, allDataFacts);
        Log(LogLevel::Info) << std::to_string(ct2) << " ";
        Dump() << std::to_string(ct2) << " ";
        const ICFGNode *curEvalICFGNode = src->getICFGNode();
        if (const RetICFGNode *retICFGNOde = dyn_cast<RetICFGNode>(curEvalICFGNode)) {
            curEvalICFGNode = retICFGNOde->getCallICFGNode();
        }
        Set<const SVFFunction *> curEvalFuns;
        for (const auto &e: curEvalICFGNode->getOutEdges()) {
            if (const CallCFGEdge *callEdge = dyn_cast<CallCFGEdge>(e)) {
                curEvalFuns.insert(callEdge->getDstNode()->getFun());
            }
        }
        buIFDSSolve(mainEntry, getICFGWrapper()->getICFGNodeWrapper(dataFact.front()), curEvalICFGNode, allDataFacts, curEvalFuns);
//            // early terminate - no seq at entry
        if ((*mainEntry->getOutEdges().begin())->getDstNode()->_buReachableDataFacts.find(allDataFacts[0]) ==
            (*mainEntry->getOutEdges().begin())->getDstNode()->_buReachableDataFacts.end())
            continue;
        initTDDFTransferFunc(mainEntry, allDataFacts);
        tdIFDSSolve(mainEntry, curEvalICFGNode, allDataFacts, curEvalFuns);

        for (const auto &n: *getICFGWrapper()) {
            if (!n.second->_tdReachableDataFacts.empty() && !n.second->_buReachableDataFacts.empty()) {
                for (const auto &df1: n.second->_tdReachableDataFacts) {
                    // has non-zero intersection datafacts
                    if (!df1.empty() &&
                        n.second->_buReachableDataFacts.find(df1) != n.second->_buReachableDataFacts.end()) {
                        temporalSliceTmp.insert(n.first);
                        temporalSlice.insert(n.first);
                    }
                }
            }
        }
        // N_t reaches snk
        if (temporalSliceTmp.find(dataFact.front()) != temporalSliceTmp.end()) {
            for (const auto &e: getICFGWrapper()->getICFGNodeWrapper(dataFact.front())->getOutEdges()) {
                temporalSlice.insert(e->getDstID());
            }
        }
    }
    Log(LogLevel::Info) << SVFUtil::sucMsg("\nTemporal slicing...[done]\n");
    Dump() << SVFUtil::sucMsg("\nTemporal slicing...[done]\n");
    _temporalSlice = SVFUtil::move(temporalSlice);
}

void TemporalSlicer::initTDDFTransferFunc(const ICFGNodeWrapper *mainEntry, std::vector<DataFact> &allDataFacts) {
    for (const auto &item: *ICFGWrapper::getICFGWrapper(PAG::getPAG()->getICFG())) {
        if (item.second == mainEntry) {
            ICFGEdgeWrapper *edge = *item.second->getOutEdges().begin();
            edge->_tdDataFactTransferFunc[DataFact()].insert(DataFact());
            edge->_tdDataFactTransferFunc[DataFact()].insert(allDataFacts[0]);
        } else {
            if (item.second->_buReachableDataFacts.empty()) continue;
            for (const auto &edge: item.second->getOutEdges()) {
                for (const auto &dataFact: allDataFacts) {
                    if (dataFact.empty()) {
                        edge->_tdDataFactTransferFunc[DataFact()].insert(DataFact());
                    } else if (edge->getSrcID() == dataFact.back()) {
                        // edge: returnsite -> nxt or load -> nxt (UAF)
                        // the seq only contains return node
                        DataFact dstFact = dataFact;
                        dstFact.pop_back();
                        if (item.second->_buReachableDataFacts.find(dataFact) !=
                            item.second->_buReachableDataFacts.end())
                            edge->_tdDataFactTransferFunc[dataFact].insert(SVFUtil::move(dstFact));
                    } else {
                        if (item.second->_buReachableDataFacts.find(dataFact) !=
                            item.second->_buReachableDataFacts.end())
                            edge->_tdDataFactTransferFunc[dataFact].insert(dataFact);
                    }
                }
            }
        }

    }
}

void TemporalSlicer::initBUDFTransferFunc(const ICFGNodeWrapper *mainEntry, std::vector<DataFact> &allDataFacts) {
    for (const auto &item: *ICFGWrapper::getICFGWrapper(PAG::getPAG()->getICFG())) {
        if (item.second == mainEntry) {
            ICFGEdgeWrapper *edge = *item.second->getOutEdges().begin();
            edge->_buDataFactTransferFunc[DataFact()].insert(DataFact());
            edge->_buDataFactTransferFunc[allDataFacts[0]].insert(DataFact());
        } else {
            for (const auto &edge: item.second->getOutEdges()) {
                for (const auto &dataFact: allDataFacts) {
                    if (dataFact.empty()) {
                        edge->_buDataFactTransferFunc[DataFact()].insert(DataFact());
                    } else if (edge->getSrcID() == dataFact.back()) {
                        // edge: returnsite -> nxt or load -> nxt (UAF)
                        // the seq only contains return node
                        DataFact dstFact = dataFact;
                        dstFact.pop_back();
                        edge->_buDataFactTransferFunc[SVFUtil::move(dstFact)].insert(dataFact);
                    } else {
                        edge->_buDataFactTransferFunc[dataFact].insert(dataFact);
                    }
                }
            }
        }

    }
}

/*!
 * Top-Down IFDS solver for tailoring
 * @param mainEntry
 * @param curEvalICFGNode
 * @param allDataFacts
 */
void TemporalSlicer::tdIFDSSolve(const ICFGNodeWrapper *mainEntry, const ICFGNode *curEvalICFGNode,
                                 std::vector<DataFact> &allDataFacts, Set<const SVFFunction *> &curEvalFuns) {
    FIFOWorkList<TPSIFDSItem> workList;
    ICFG *icfg = PAG::getPAG()->getICFG();
    Map<const SVFFunction *, Map<DataFact, Set<DataFact>>> summaryMap;
    Set<TPSIFDSItem> pathEdge;
    TPSIFDSItem firstItem(mainEntry, DataFact(), mainEntry, DataFact());
    workList.push(firstItem);
    pathEdge.insert(firstItem);
    while (!workList.empty()) {
        TPSIFDSItem curItem = workList.pop();
        if (const CallICFGNode *callICFGNode = isCallNode(curItem.getDst().first, curEvalICFGNode, curEvalFuns)) {
            for (const auto &edge: curItem.getDst().first->getOutEdges()) {
                // add trigger
                if (edge->getICFGEdge()->isCallCFGEdge()) {
                    auto it2 = edge->_tdDataFactTransferFunc.find(curItem.getDst().second);
                    if (it2 == edge->_tdDataFactTransferFunc.end()) continue;
                    // for each <n, d2> --> <callee entry, d3>
                    for (const auto &dataFact: it2->second) {
                        TPSIFDSItem nxt(edge->getDstNode(), dataFact, edge->getDstNode(), dataFact);
                        propagate(workList, pathEdge, nxt);
                    }
                }
                // callsite --> returnsite in E^#
                if (edge->getDstNode() == curItem.getDst().first->getRetICFGNodeWrapper()) {
                    auto it2 = edge->_tdDataFactTransferFunc.find(curItem.getDst().second);
                    if (it2 == edge->_tdDataFactTransferFunc.end()) continue;
                    for (const auto &dataFact: it2->second) {
                        TPSIFDSItem nxt(curItem.getSrc().first, curItem.getSrc().second, edge->getDstNode(),
                                        dataFact);
                        propagate(workList, pathEdge, nxt);
                    }
                }
            }
            Set<const SVFFunction *> functionSet;
            getPTACallGraph()->getCallees(callICFGNode, functionSet);
            if (functionSet.empty())
                continue;
            auto summaryIt = summaryMap.find(*functionSet.begin());
            // have summary
            if (summaryIt != summaryMap.end()) {
                auto it = summaryIt->second.find(curItem.getDst().second);
                if (it != summaryIt->second.end())
                    for (const auto &dataFact: it->second) {
                        TPSIFDSItem nxt(curItem.getSrc().first, curItem.getSrc().second,
                                        curItem.getDst().first->getRetICFGNodeWrapper(),
                                        dataFact);
                        propagate(workList, pathEdge, nxt);
                    }
            }
        } else if (const FunExitICFGNode *funExitICFGNode = isExitNode(curItem.getDst().first)) {
            std::vector<const ICFGNodeWrapper *> retNodes;
            returnSites(curItem.getDst().first, retNodes);
            for (const auto &retsite: retNodes) {
                const ICFGNodeWrapper *callsite = retsite->getCallICFGNodeWrapper();
                Set<DataFact> d4s;
                for (const auto &e: callsite->getOutEdges()) {
                    if (e->getICFGEdge()->isCallCFGEdge()) {
                        for (const auto &item: e->_tdDataFactTransferFunc) {
                            // item.first is d4
                            if (item.second.find(curItem.getSrc().second) != item.second.end()) {
                                d4s.insert(item.first);
                            }
                        }
                    }
                }
                Set<DataFact> d5s;
                for (const auto &e: curItem.getDst().first->getOutEdges()) {
                    if (e->getICFGEdge()->isRetCFGEdge()) {
                        auto it2 = e->_tdDataFactTransferFunc.find(curItem.getDst().second);
                        d5s = it2->second;
                    }
                }
                for (const auto &d4: d4s) {
                    for (const auto &d5: d5s) {
                        summaryMap[funExitICFGNode->getFun()][d4].insert(d5);
                        const ICFGNodeWrapper *funEntryIcfgNode = getICFGWrapper()->getFunEntry(
                                retsite->getICFGNode()->getFun());
                        for (const auto &dataFact: allDataFacts) {
                            TPSIFDSItem item(funEntryIcfgNode, dataFact, callsite, d4);
                            if (pathEdge.find(item) != pathEdge.end()) {
                                TPSIFDSItem item2(funEntryIcfgNode, dataFact, retsite, d5);
                                propagate(workList, pathEdge, item2);
                            }
                        }
                    }
                }
            }

        } else {
            if (const CallICFGNode *callICFGNode = dyn_cast<CallICFGNode>(curItem.getDst().first->getICFGNode())) {
                const ICFGNodeWrapper *retIcfgNode = curItem.getDst().first->getRetICFGNodeWrapper();
                pathEdge.insert(TPSIFDSItem(curItem.getSrc().first, curItem.getSrc().second, retIcfgNode,
                                            curItem.getDst().second));
                for (const auto &e: retIcfgNode->getOutEdges()) {
                    auto it2 = e->_tdDataFactTransferFunc.find(curItem.getDst().second);
                    if (it2 != e->_tdDataFactTransferFunc.end()) {
                        for (const auto &d3: it2->second) {
                            TPSIFDSItem nxt(curItem.getSrc().first, curItem.getSrc().second, e->getDstNode(), d3);
                            propagate(workList, pathEdge, nxt);
                        }
                    }
                }
            } else {
                for (const auto &e: curItem.getDst().first->getOutEdges()) {
                    auto it2 = e->_tdDataFactTransferFunc.find(curItem.getDst().second);
                    if (it2 != e->_tdDataFactTransferFunc.end()) {
                        for (const auto &d3: it2->second) {
                            TPSIFDSItem nxt(curItem.getSrc().first, curItem.getSrc().second, e->getDstNode(), d3);
                            propagate(workList, pathEdge, nxt);
                        }
                    }
                }
            }
        }
    }
    for (const auto &n: *getICFGWrapper()) {
        for (const auto &d1: allDataFacts) {
            for (const auto &d2: allDataFacts) {
                if (!d2.empty() && n.second->getICFGNode()->getFun()) {
                    TPSIFDSItem item(getICFGWrapper()->getFunEntry(n.second->getICFGNode()->getFun()), d1, n.second,
                                     d2);
                    if (pathEdge.find(item) != pathEdge.end()) {
                        n.second->_tdReachableDataFacts.insert(d2);
                    }
                }
            }
        }
    }

}

void TemporalSlicer::connectTowardsMainExit(const ICFGNodeWrapper *snkExit, Set<TPSIFDSItem> &pathEdge) {
    FIFOWorkList<const ICFGNodeWrapper *> workList;
    Set<const ICFGNodeWrapper *> visited;
    for (const auto &outEdge: snkExit->getOutEdges()) {
        if (SVFUtil::isa<RetCFGEdge>(outEdge->getICFGEdge())) {
            workList.push(outEdge->getDstNode());
            visited.insert(outEdge->getDstNode());
        }
    }
    while (!workList.empty()) {
        const ICFGNodeWrapper *curNode = workList.pop();
        if (const SVFFunction *fun = curNode->getICFGNode()->getFun()) {
            const ICFGNodeWrapper *funExit = getICFGWrapper()->getFunExit(fun);
            pathEdge.insert(TPSIFDSItem(funExit, DataFact(), curNode, DataFact()));
            for (const auto &outEdge: funExit->getOutEdges()) {
                if (SVFUtil::isa<RetCFGEdge>(outEdge->getICFGEdge()) && !visited.count(outEdge->getDstNode())) {
                    workList.push(outEdge->getDstNode());
                    visited.insert(outEdge->getDstNode());
                }
            }
        }
    }
}
/*!
 * Bottom-Up IFDS solver for tailoring
 * @param mainEntry
 * @param curEvalICFGNode
 * @param allDataFacts
 */
void TemporalSlicer::buIFDSSolve(const ICFGNodeWrapper *mainEntry, const ICFGNodeWrapper *snk, const ICFGNode *curEvalICFGNode,
                                 std::vector<DataFact> &allDataFacts, Set<const SVFFunction *> &curEvalFuns) {
    FIFOWorkList<TPSIFDSItem> workList;
    ICFG *icfg = PAG::getPAG()->getICFG();
    Map<const SVFFunction *, Map<DataFact, Set<DataFact>>> summaryMap;
    Set<TPSIFDSItem> pathEdge;
//    const ICFGNodeWrapper *mainExit = getICFGWrapper()->getFunExit(mainEntry->getICFGNode()->getFun());
//    TPSIFDSItem firstItem(mainExit, DataFact(), mainExit, DataFact());
    const ICFGNodeWrapper *snkExit = getICFGWrapper()->getFunExit(snk->getICFGNode()->getFun());
    connectTowardsMainExit(snkExit, pathEdge);
    TPSIFDSItem firstItem(snkExit, DataFact(), snk, DataFact{snk->getId()});
    workList.push(firstItem);
    pathEdge.insert(firstItem);
    while (!workList.empty()) {
        TPSIFDSItem curItem = workList.pop();
        if (const RetICFGNode *retICFGNode = isRetNode(curItem.getDst().first, curEvalICFGNode, curEvalFuns)) {
            for (const auto &edge: curItem.getDst().first->getInEdges()) {
                // add trigger
                if (edge->getICFGEdge()->isRetCFGEdge()) {
                    auto it2 = edge->_buDataFactTransferFunc.find(curItem.getDst().second);
                    // for each <n, d2> --> <callee entry, d3>
                    for (const auto &dataFact: it2->second) {
                        TPSIFDSItem nxt(edge->getSrcNode(), dataFact, edge->getSrcNode(), dataFact);
                        propagate(workList, pathEdge, nxt);
                    }
                }
                // callsite --> returnsite in E^#
                if (edge->getSrcNode() == curItem.getDst().first->getCallICFGNodeWrapper()) {
                    auto it2 = edge->_buDataFactTransferFunc.find(curItem.getDst().second);
                    for (const auto &dataFact: it2->second) {
                        TPSIFDSItem nxt(curItem.getSrc().first, curItem.getSrc().second, edge->getSrcNode(),
                                        dataFact);
                        propagate(workList, pathEdge, nxt);
                    }
                }
            }
            Set<const SVFFunction *> functionSet;
            getPTACallGraph()->getCallees(retICFGNode->getCallICFGNode(), functionSet);
            if (functionSet.empty())
                continue;
            auto summaryIt = summaryMap.find(*functionSet.begin());
            // have summary
            if (summaryIt != summaryMap.end()) {
                auto it = summaryIt->second.find(curItem.getDst().second);
                if (it != summaryIt->second.end())
                    for (const auto &dataFact: it->second) {
                        TPSIFDSItem nxt(curItem.getSrc().first, curItem.getSrc().second,
                                        curItem.getDst().first->getCallICFGNodeWrapper(),
                                        dataFact);
                        propagate(workList, pathEdge, nxt);
                    }
            }
        } else if (const FunEntryICFGNode *funEntryICFGNode = isEntryNode(curItem.getDst().first)) {
            std::vector<const ICFGNodeWrapper *> callNodes;
            callSites(curItem.getDst().first, callNodes);
            for (const auto &callsite: callNodes) {
                const ICFGNodeWrapper *retsite = callsite->getRetICFGNodeWrapper();
                Set<DataFact> d4s;
                for (const auto &e: retsite->getInEdges()) {
                    if (e->getICFGEdge()->isRetCFGEdge()) {
                        for (const auto &item: e->_buDataFactTransferFunc) {
                            // item.first is d4
                            if (item.second.find(curItem.getSrc().second) != item.second.end()) {
                                d4s.insert(item.first);
                            }
                        }
                    }
                }
                Set<DataFact> d5s;
                for (const auto &e: curItem.getDst().first->getInEdges()) {
                    if (e->getICFGEdge()->isCallCFGEdge()) {
                        auto it2 = e->_buDataFactTransferFunc.find(curItem.getDst().second);
                        d5s = it2->second;
                    }
                }
                for (const auto &d4: d4s) {
                    for (const auto &d5: d5s) {
                        summaryMap[funEntryICFGNode->getFun()][d4].insert(d5);
                        const ICFGNodeWrapper *funExitIcfgNode = getICFGWrapper()->getFunExit(
                                callsite->getICFGNode()->getFun());
                        for (const auto &dataFact: allDataFacts) {
                            TPSIFDSItem item(funExitIcfgNode, dataFact, retsite, d4);
                            if (pathEdge.find(item) != pathEdge.end()) {
                                TPSIFDSItem item2(funExitIcfgNode, dataFact, callsite, d5);
                                propagate(workList, pathEdge, item2);
                            }
                        }
                    }
                }
            }

        } else {
            if (const RetICFGNode *retICFGNode = dyn_cast<RetICFGNode>(curItem.getDst().first->getICFGNode())) {
                const ICFGNodeWrapper *callIcfgNode = curItem.getDst().first->getCallICFGNodeWrapper();
                pathEdge.insert(TPSIFDSItem(curItem.getSrc().first, curItem.getSrc().second, callIcfgNode,
                                            curItem.getDst().second));
                for (const auto &e: callIcfgNode->getInEdges()) {
                    auto it2 = e->_buDataFactTransferFunc.find(curItem.getDst().second);
                    if (it2 != e->_buDataFactTransferFunc.end()) {
                        for (const auto &d3: it2->second) {
                            TPSIFDSItem nxt(curItem.getSrc().first, curItem.getSrc().second, e->getSrcNode(), d3);
                            propagate(workList, pathEdge, nxt);
                        }
                    }
                }
            } else {
                for (const auto &e: curItem.getDst().first->getInEdges()) {
                    auto it2 = e->_buDataFactTransferFunc.find(curItem.getDst().second);
                    if (it2 != e->_buDataFactTransferFunc.end()) {
                        for (const auto &d3: it2->second) {
                            TPSIFDSItem nxt(curItem.getSrc().first, curItem.getSrc().second, e->getSrcNode(), d3);
                            propagate(workList, pathEdge, nxt);
                        }
                    }
                }
            }
        }
    }
    for (const auto &n: *getICFGWrapper()) {
        for (const auto &d1: allDataFacts) {
            for (const auto &d2: allDataFacts) {
                if (!d2.empty() && n.second->getICFGNode()->getFun()) {
                    TPSIFDSItem item(getICFGWrapper()->getFunExit(n.second->getICFGNode()->getFun()), d1, n.second, d2);
                    if (pathEdge.find(item) != pathEdge.end()) {
                        n.second->_buReachableDataFacts.insert(d2);
                    }
                }
            }
        }
    }

}