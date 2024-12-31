//
// Created by Xiao on 2022/3/25.
//

#include "PSAStat.h"
#include "PSAOptions.h"
#include <fstream>
#include <numeric>
#include <queue>
#include "Logger.h"

using namespace SVF;
using namespace SVFUtil;
using namespace std;

PSAStat::PSAStat(PSTA *_psta) : esp(_psta) {
    assert((PSAOptions::ClockType() == PTAStat::ClockType::Wall || PSAOptions::ClockType() == PTAStat::ClockType::CPU)
           && "PSAStat: unknown clock type!");
    startClk();
}

double PSAStat::getClk(bool mark) {
    if (PSAOptions::MarkedClocksOnly() && !mark) return 0.0;

    if (PSAOptions::ClockType() == PTAStat::ClockType::Wall) {
        struct timespec time;
        clock_gettime(CLOCK_MONOTONIC, &time);
        return (double) (time.tv_nsec + time.tv_sec * 1000000000) / 1000000.0;
    } else if (PSAOptions::ClockType() == PTAStat::ClockType::CPU) {
        return CLOCK_IN_MS();
    }

    assert(false && "PTAStat::getClk: unknown clock type");
    abort();
}

void PSAStat::performStat(string model) {
    endClk();
    memUsage = getMemUsage();
    paramNumMap["Cxt Alias"] = PSAOptions::CxtSensitiveAlias();
    paramNumMap["OTF Alias"] = PSAOptions::OTFAlias();
//    paramNumMap["FieldSensitive"] = PSAOptions::FiSensi();
    paramNumMap["Cxt Limit"] = PSAOptions::CxtLimit();
    paramNumMap["Multi Slicing"] = PSAOptions::MultiSlicing();
    paramNumMap["Enable Spatial Slicing"] = PSAOptions::EnableSpatialSlicing();
    paramNumMap["Enable Temporal Slicing"] = PSAOptions::EnableTemporalSlicing();
    paramNumMap["Enable Isolated Summary"] = PSAOptions::EnableIsoSummary();
    paramNumMap["Snk Limit"] = PSAOptions::MaxSnkLimit();
    paramNumMap["Max Z3 Size"] = PSAOptions::MaxBoolNum();
    paramNumMap["Max PI Size"] = PSAOptions::MaxSQSize();
//    paramNumMap["ExeState"] = PSAOptions::ExeStateType();
    paramNumMap["Max Step In Wrapper"] = PSAOptions::MaxStepInWrapper();
    paramNumMap["Spatial Layer Num"] = PSAOptions::LayerNum();


    timeStatMap["InitSrc"] = (getsrcEndTime - getsrcStartTime) / TIMEINTERVAL;
    timeStatMap["InitAbsTransferFunc"] = absTranserFuncTotalTime / TIMEINTERVAL;
    timeStatMap["Solve"] = (solveEndTime - solveStartTime) / TIMEINTERVAL;
    timeStatMap["Compact Graph Time"] = compactGraphTotalTime / TIMEINTERVAL;
    timeStatMap["WrapICFGTime"] = (wrapEndTime - wrapStartTime) / TIMEINTERVAL;

    generalNumMap["SrcNum"] = esp->_srcs.size();
    generalNumMap["AbsState Domain Size"] = esp->getFSMParser()->getAbsStates().size();

    generalNumMap["Var Avg Num"] =
            std::accumulate(_varAvgSZ.begin(), _varAvgSZ.end(), 0.0) / _varAvgSZ.size();
    generalNumMap["Loc Avg Num"] =
            std::accumulate(_locAvgSZ.begin(), _locAvgSZ.end(), 0.0) / _locAvgSZ.size();

    generalNumMap["Var Addr Avg Num"] =
            std::accumulate(_varAddrAvgSZ.begin(), _varAddrAvgSZ.end(), 0.0) / _varAddrAvgSZ.size();
    generalNumMap["Loc Addr Avg Num"] =
            std::accumulate(_locAddrAvgSZ.begin(), _locAddrAvgSZ.end(), 0.0) / _locAddrAvgSZ.size();

    generalNumMap["Var AddrSet Avg Size"] =
            std::accumulate(_varAddrSetAvgSZ.begin(), _varAddrSetAvgSZ.end(), 0.0) / _varAddrSetAvgSZ.size();
    generalNumMap["Loc AddrSet Avg Size"] =
            std::accumulate(_locAddrSetAvgSZ.begin(), _locAddrSetAvgSZ.end(), 0.0) / _locAddrSetAvgSZ.size();


    generalNumMap["Info Map Avg Size"] =
            std::accumulate(_infoMapSzs.begin(), _infoMapSzs.end(), 0.0) / _infoMapSzs.size();
    generalNumMap["Summary Map Avg Size"] =
            std::accumulate(_summaryMapSzs.begin(), _summaryMapSzs.end(), 0.0) / _summaryMapSzs.size();
    generalNumMap["Graph Avg Node Num"] =
            std::accumulate(_nodeNums.begin(), _nodeNums.end(), 0.0) / _nodeNums.size();
    generalNumMap["Graph Avg Edge Num"] =
            std::accumulate(_edgeNums.begin(), _edgeNums.end(), 0.0) / _edgeNums.size();
    double trackingBranchNum = 0;
    if (esp->_graphSparsificator._srcToBranch.size() != 0) {
        for (const auto &item: esp->_graphSparsificator._srcToBranch) {
            trackingBranchNum += (((double) item.second.size()) / esp->_graphSparsificator._srcToBranch.size());
        }
    }

    generalNumMap["Tracking Branch Avg Num"] = trackingBranchNum;
    generalNumMap["ICFG Node Num"] = esp->_icfg->getTotalNodeNum();
    Set<const ICFGEdge *> icfgEdges;
    for (const auto &n: *esp->_icfg) {
        for (const auto &e: n.second->getOutEdges()) {
            icfgEdges.insert(e);
        }
    }
    generalNumMap["ICFG Edge Num"] = icfgEdges.size();
    generalNumMap["Branch Num"] = esp->getPathAllocator()->getIcfgNodeCondsNum();

    if (!PSAOptions::MultiSlicing()) {
        timeStatMap["Collecting Call Time"] = triggerCallTotalTime / TIMEINTERVAL;
        timeStatMap["Tracking Branch Time"] = trackingBranchTotalTime / TIMEINTERVAL;
        double callSiteAvg = 0;
        if (esp->_graphSparsificator._srcToCallsites.size() != 0) {
            for (const auto &item: esp->_graphSparsificator._srcToCallsites) {
                callSiteAvg += (((double) item.second.size()) / esp->_graphSparsificator._srcToCallsites.size());
            }
        }
        generalNumMap["Callsites Avg Num"] = callSiteAvg;
        double N_sNum = 0;
        if (esp->_graphSparsificator._srcToSpatialSlice.size() != 0) {
            for (const auto &item: esp->_graphSparsificator._srcToSpatialSlice) {
                N_sNum += (((double) item.second.size()) / esp->_graphSparsificator._srcToSpatialSlice.size());
            }
        }
        generalNumMap["Spatial Avg Num"] = N_sNum;
    }

    if (PSAOptions::MultiSlicing()) {
        double NtNum = 0;
        if (esp->_graphSparsificator._srcToTemporalSlice.size() != 0) {
            for (const auto &item: esp->_graphSparsificator._srcToTemporalSlice) {
                NtNum += (((double) item.second.size()) / esp->_graphSparsificator._srcToTemporalSlice.size());
            }
        }
        generalNumMap["Temporal Avg Num"] = NtNum;
        double NsNum = 0;
        if (esp->_graphSparsificator._srcToSpatialSlice.size() != 0) {
            for (const auto &item: esp->_graphSparsificator._srcToSpatialSlice) {
                NsNum += (((double) item.second.size()) / esp->_graphSparsificator._srcToSpatialSlice.size());
            }
        }
        generalNumMap["Spatial Avg Num"] = NsNum;
        double NcNum = 0;
        if (esp->_graphSparsificator._srcToCallsites.size() != 0) {
            for (const auto &item: esp->_graphSparsificator._srcToCallsites) {
                NcNum += (((double) item.second.size()) / esp->_graphSparsificator._srcToCallsites.size());
            }
        }
        generalNumMap["Callsites Avg Num"] = NcNum;
        double SQNum = 0;
        if (esp->_graphSparsificator._srcToPI.size() != 0) {
            for (const auto &item: esp->_graphSparsificator._srcToPI) {
                SQNum += (((double) item.second.size()) / esp->_graphSparsificator._srcToPI.size());
            }
        }
        generalNumMap["PI Avg Num"] = SQNum;

        timeStatMap["PI Generation Time"] = seqsExtractTotalTime / TIMEINTERVAL;
        timeStatMap["Temporal Time"] = ntExtractTotalTime / TIMEINTERVAL;
        timeStatMap["Temporal Avg Time"] = timeStatMap["Temporal Time"] / SQNum;
        timeStatMap["Spatial Time"] = nsExtractTotalTime / TIMEINTERVAL;
        timeStatMap["Callsites Time"] = ncExtractTotalTime / TIMEINTERVAL;
    }
    generalNumMap["Bug Num"] = _bugNum;

    timeStatMap["TotalTime"] = (endTime - startTime) / TIMEINTERVAL;

    if (PSAOptions::PrintStat()) {
        printStat(model);
    }
    if (PSAOptions::DumpState())
        dumpStat(model);
}

void PSAStat::printStat(string statname) {

    string fullName(SymbolTableInfo::SymbolInfo()->getModule()->getModuleIdentifier());
    string name = fullName.substr(fullName.find('/'), fullName.size());
    moduleName = name.substr(0, fullName.find('.'));
    SVFUtil::outs() << "\n*********" << statname << "***************\n";
    SVFUtil::outs() << "################ (program : " << moduleName << ")###############\n";
    SVFUtil::outs().flags(std::ios::left);
    unsigned field_width = 30;
    for (NUMStatMap::iterator it = paramNumMap.begin(), eit = paramNumMap.end(); it != eit; ++it) {
        // format out put with width 20 space
        std::cout << std::setw(field_width) << it->first << it->second << "\n";
    }
    SVFUtil::outs() << "-------------------------------------------------------\n";
    for (NUMStatMap::iterator it = generalNumMap.begin(), eit = generalNumMap.end(); it != eit; ++it) {
        // format out put with width 20 space
        std::cout << std::setw(field_width) << it->first << it->second << "\n";
    }
    SVFUtil::outs() << "-------------------------------------------------------\n";
    for (TIMEStatMap::iterator it = timeStatMap.begin(), eit = timeStatMap.end(); it != eit; ++it) {
        // format out put with width 20 space
        SVFUtil::outs() << std::setw(field_width) << it->first << it->second << "s\n";
    }
    SVFUtil::outs() << "Memory usage: " << memUsage << "\n";

    SVFUtil::outs() << "#######################################################" << std::endl;
    SVFUtil::outs().flush();
}

void PSAStat::dumpStat(std::string statname) {
    string fullName(SymbolTableInfo::SymbolInfo()->getModule()->getModuleIdentifier());
    string name = fullName.substr(fullName.find('/'), fullName.size());
    moduleName = name.substr(0, fullName.find('.'));

    Dump() << "\n*********" << statname << "***************\n";
    Dump() << "################ (program : " << moduleName << ")###############\n";
    Dump().flags(std::ios::left);
    unsigned field_width = 30;
    for (NUMStatMap::iterator it = paramNumMap.begin(), eit = paramNumMap.end(); it != eit; ++it) {
        // format out put with width 20 space
        Dump() << std::setw(field_width) << it->first << it->second << "\n";
    }
    Dump() << "-------------------------------------------------------\n";
    for (NUMStatMap::iterator it = generalNumMap.begin(), eit = generalNumMap.end(); it != eit; ++it) {
        // format out put with width 20 space
        Dump() << std::setw(field_width) << it->first << it->second << "\n";
    }
    Dump() << "-------------------------------------------------------\n";
    for (TIMEStatMap::iterator it = timeStatMap.begin(), eit = timeStatMap.end(); it != eit; ++it) {
        // format out put with width 20 space
        Dump() << std::setw(field_width) << it->first << it->second << "s\n";
    }
    Dump() << "memory usage: " << memUsage << "\n";

    Dump() << "#######################################################\n";
}

void PSAStat::collectCompactedGraphStats() {
    ICFGNodeWrapper *mainEntryNode = esp->getICFGWrapper()->getICFGNodeWrapper(esp->_mainEntry->getId());
    std::queue<const ICFGNodeWrapper *> queue;
    Set<const ICFGNodeWrapper *> visited;
    Set<const ICFGEdgeWrapper *> visitedEdge;
    queue.push(mainEntryNode);
    visited.insert(mainEntryNode);
    while (!queue.empty()) {
        const ICFGNodeWrapper *cur = queue.front();
        queue.pop();
        if (const CallICFGNode *callNode = esp->isCallNode(cur)) {
            PSTA::FunctionSet funSet;
            esp->callee(callNode, funSet);
            for (const auto &fun: funSet) {
                const ICFGNodeWrapper *entry = esp->entryNode(fun);
                for (const auto &entryE: entry->getOutEdges()) {
                    visitedEdge.insert(entryE);
                    if (!visited.count(entryE->getDstNode())) {
                        visited.insert(entryE->getDstNode());
                        queue.push(entryE->getDstNode());
                    }
                }
            }
        } else if (esp->isExitNode(cur)) {
            for (const auto &e: cur->getOutEdges()) {
                if (!visited.count(e->getDstNode())) {
                    visited.insert(e->getDstNode());
                    queue.push(e->getDstNode());
                }
            }
        } else {
            if (cur->getRetICFGNodeWrapper()) cur = cur->getRetICFGNodeWrapper();
            for (const auto &e: cur->getOutEdges()) {
                visitedEdge.insert(e);
                if (!visited.count(e->getDstNode())) {
                    visited.insert(e->getDstNode());
                    queue.push(e->getDstNode());
                }
            }
        }
    }
    _nodeNums.push_back(visited.size());
    _edgeNums.push_back(visitedEdge.size());

    Set<const ICFGEdgeWrapper *> infoEdge;
    Set<const SVFFunction *> summaryFunc;
    for (const auto &item: esp->_infoMap) {
        infoEdge.insert(item.first.first);
    }
    for (const auto &item: esp->_summaryMap) {
        summaryFunc.insert(item.first.first);
    }
    _infoMapSzs.push_back(infoEdge.size());
    _summaryMapSzs.push_back(summaryFunc.size());

    double infoSZ = 0;
    for (const auto &item: esp->_infoMap) {
        for (const auto &item2: item.second) {
            if (!item2.second.isNullSymState()) infoSZ++;
        }
    }

    double varAvgSZ = 0;
    double locAvgSZ = 0;
    for (const auto &item: esp->_infoMap) {
        for (const auto &item2: item.second) {
            varAvgSZ += (item2.second.getExecutionState().getVarToVal().size() / infoSZ);
            locAvgSZ += (item2.second.getExecutionState().getLocToVal().size() / infoSZ);
        }
    }
    _varAvgSZ.push_back(varAvgSZ);
    _locAvgSZ.push_back(locAvgSZ);

    double varAddrAvgSZ = 0;
    double locAddrAvgSZ = 0;
    std::vector<double> varAddrSetNums;
    std::vector<double> locAddrSetNums;

    for (const auto &item: esp->_infoMap) {
        for (const auto &item2: item.second) {
            varAddrAvgSZ += (item2.second.getExecutionState().getVarToAddrs().size() / infoSZ);
            locAddrAvgSZ += (item2.second.getExecutionState().getLocToAddrs().size() / infoSZ);
            double varAddrSetAvgSZ = 0;
            double locAddrSetAvgSZ = 0;
            for (const auto &it: item2.second.getExecutionState().getVarToAddrs()) {
                varAddrSetAvgSZ += (it.second.size() / item2.second.getExecutionState().getVarToAddrs().size());
            }
            for (const auto &it: item2.second.getExecutionState().getLocToAddrs()) {
                locAddrSetAvgSZ += (it.second.size() / item2.second.getExecutionState().getLocToAddrs().size());
            }
            varAddrSetNums.push_back(varAddrSetAvgSZ);
            locAddrSetNums.push_back(locAddrSetAvgSZ);
        }
    }
    _varAddrAvgSZ.push_back(varAddrAvgSZ);
    _locAddrAvgSZ.push_back(locAddrAvgSZ);

    _varAddrSetAvgSZ.push_back(
            std::accumulate(varAddrSetNums.begin(), varAddrSetNums.end(), 0.0) / varAddrSetNums.size());
    _locAddrSetAvgSZ.push_back(
            std::accumulate(locAddrSetNums.begin(), locAddrSetNums.end(), 0.0) / locAddrSetNums.size());
}