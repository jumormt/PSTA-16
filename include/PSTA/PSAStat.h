//
// Created by Xiao on 2022/3/25.
//

#ifndef PSA_PSASTAT_H
#define PSA_PSASTAT_H

#include <string>
#include "SVF-LLVM/BasicTypes.h"
#include "PSTA/PSTA.h"

namespace SVF {
class PSAStat {
public:

    typedef OrderedMap<const char *, double> TIMEStatMap;
    typedef OrderedMap<const char *, u32_t> NUMStatMap;


    enum ClockType {
        Wall,
        CPU,
    };

    PSAStat(PSTA *_psta);

    virtual ~PSAStat() {

    }

private:
    PSTA *esp;
    std::string moduleName;
    double startTime{0};
    double endTime{0};

    double getsrcStartTime{0};
    double getsrcEndTime{0};

    double absTranserFuncStartTime{0};
    double absTranserFuncTotalTime{0};

    double solveStartTime{0};
    double solveEndTime{0};

    double triggerCallStartTime{0};
    double triggerCallTotalTime{0};

    double trackingBranchStartTime{0};
    double trackingBranchTotalTime{0};

    double seqsExtractStartTime{0};
    double seqsExtractTotalTime{0};

    double ntExtractStartTime{0};
    double ntExtractTotalTime{0};

    double nsExtractStartTime{0};
    double nsExtractTotalTime{0};

    double ncExtractStartTime{0};
    double ncExtractTotalTime{0};

    double wrapStartTime{0};
    double wrapEndTime{0};

    double compactGraphStartTime{0};
    double compactGraphTotalTime{0};

    TIMEStatMap timeStatMap;
    NUMStatMap generalNumMap;
    NUMStatMap paramNumMap;

    std::string memUsage;

    std::vector<u32_t> _nodeNums, _edgeNums, _infoMapSzs, _summaryMapSzs;
    std::vector<double> _varAvgSZ, _locAvgSZ, _varAddrAvgSZ, _locAddrAvgSZ, _varAddrSetAvgSZ, _locAddrSetAvgSZ;
    u32_t _bugNum{0};


public:
    virtual inline void startClk() {
        startTime = getClk(true);
    }

    virtual inline void endClk() {
        endTime = getClk(true);
    }

    void getSrcStart() {
        getsrcStartTime = getClk(true);
    }

    void getSrcEnd() {
        getsrcEndTime = getClk(true);
    }

    void wrapStart() {
        wrapStartTime = getClk(true);
    }

    void wrapEnd() {
        wrapEndTime = getClk(true);
    }

    void absTranserFuncStart() {
        absTranserFuncStartTime = getClk(true);
    }

    void absTranserFuncEnd() {
        absTranserFuncTotalTime += (getClk(true) - absTranserFuncStartTime);
    }

    void solveStart() {
        solveStartTime = getClk(true);
    }

    void solveEnd() {
        solveEndTime = getClk(true);
    }

    void ntStart() {
        ntExtractStartTime = getClk(true);
    }

    void ntEnd() {
        ntExtractTotalTime += (getClk(true) - ntExtractStartTime);
    }

    void seqsStart() {
        seqsExtractStartTime = getClk(true);
    }

    void seqsEnd() {
        seqsExtractTotalTime += (getClk(true) - seqsExtractStartTime);
    }

    void nsStart() {
        nsExtractStartTime = getClk(true);
    }

    void nsEnd() {
        nsExtractTotalTime += (getClk(true) - nsExtractStartTime);
    }

    void ncStart() {
        ncExtractStartTime = getClk(true);
    }

    void ncEnd() {
        ncExtractTotalTime += (getClk(true) - ncExtractStartTime);
    }

    void trackingBranchStart() {
        trackingBranchStartTime = getClk(true);
    }

    void trackingBranchEnd() {
        trackingBranchTotalTime += (getClk(true) - trackingBranchStartTime);
    }

    void triggerCallStart() {
        triggerCallStartTime = getClk(true);
    }

    void triggerCallEnd() {
        triggerCallTotalTime += (getClk(true) - triggerCallStartTime);
    }

    void compactGraphStart() {
        compactGraphStartTime = getClk(true);
    }

    void compactGraphEnd() {
        compactGraphTotalTime += (getClk(true) - compactGraphStartTime);
    }

    inline std::string getMemUsage() {
        u32_t vmrss, vmsize;
        if (SVFUtil::getMemoryUsageKB(&vmrss, &vmsize))
            return std::to_string(vmsize) + "KB";
        else
            return "cannot read memory usage";
    }

    /// When mark is true, real clock is always returned. When mark is false, it is
    /// only returned when PSAOptions::MarkedClocksOnly is not set.
    /// Default call for getClk is unmarked, while MarkedClocksOnly is false by default.
    static double getClk(bool mark = false);

    virtual void printStat(std::string str = "");

    virtual void dumpStat(std::string str = "");

    virtual void performStat(std::string model = "");

    void collectCompactedGraphStats();

    inline void incBugNum() {
        _bugNum++;
    }

};
}


#endif //PSA_PSASTAT_H
