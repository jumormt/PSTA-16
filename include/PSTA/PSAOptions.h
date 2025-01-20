//
// Created by Xiao on 2022/3/14.
//

#ifndef PSA_PSAOPTIONS_H
#define PSA_PSAOPTIONS_H


#include "Util/Options.h"

namespace SVF {
class PSAOptions : public Options {
public:
    /// If set, use context sensitive alias analysis
    static const Option<bool> CxtSensitiveAlias;
    static const Option<bool> CxtSensitiveSpatialSlicing;
    static const Option<bool> OTFAlias;

    static const Option<bool> MultiSlicing;
    static const Option<bool> EnableSpatialSlicing;
    static const Option<bool> SSlicingNorm;
    static const Option<bool> EnableTemporalSlicing;
    static const Option<bool> Wrapper;
    static const Option<bool> PrintStat;

    static const Option<bool> DumpICFGWrapper;
    static const Option<bool> PathSensitive;

    static const Option<bool> EnableLog;
    static const Option<bool> EnableWarn;
    static const Option<bool> EnableReport;
    static const Option<bool> LoadAsUse;
    static const Option<bool> EnableIsoSummary;
    static const Option<bool> EnableDataSlicing;
    static const Option<bool> EnableExtCallSlicing;
    static const Option<bool> DumpState;
    static const Option<bool> LEAK;
    static const Option<bool> UAF;
    static const Option<bool> DF;
    static const Option<bool> Base;

    static const Option<u32_t> MaxSnkLimit;
    static const Option<u32_t> MaxSrcLimit;
    static const Option<u32_t> LayerNum;
    static const Option<u32_t> MaxBoolNum;
    static const Option<u32_t> MaxSymbolSize;
    static const Option<u32_t> MaxSQSize;
    static const Option<u32_t> EvalNode;
    static const Option<u32_t> MaxAddrs;

    static const Option<std::string> LogLevel;

    static const Option<std::string> FSMFILE;
    static const Option<std::string> OUTPUT;

};
}


#endif //PSA_PSAOPTIONS_H
