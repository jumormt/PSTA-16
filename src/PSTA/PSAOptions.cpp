//
// Created by Xiao on 2022/3/14.
//

#include "PSAOptions.h"

namespace SVF {

const Option<bool> PSAOptions::CxtSensitiveAlias(
        "cxt-alias",
        "Use context sensitive alias analysis",
        true
);

const Option<bool> PSAOptions::CxtSensitiveSpatialSlicing(
        "cxt-spatial-slicing",
        "Use context sensitive spatial slicing",
        true
);
const Option<bool> PSAOptions::OTFAlias(
        "otf-alias",
        "Use on-the-fly alias analysis",
        false
);
//const Option<bool> PSAOptions::FiSensi(
//        "fi-sensi",
//        "Field sensitive for param",
//        false);
const Option<bool> PSAOptions::MultiSlicing(
        "mul",
        "Use multi point slicing",
        false);
const Option<bool> PSAOptions::EnableSpatialSlicing(
        "spatial",
        "Use spatial multi point slicing",
        true);
const Option<bool> PSAOptions::SSlicingNorm(
        "sn",
        "Use normal spatial multi point slicing",
        false);
const Option<bool> PSAOptions::EnableTemporalSlicing(
        "temporal",
        "Use temporal multi point slicing",
        true);
const Option<bool> PSAOptions::PrintStat(
        "print-stat",
        "Print stat",
        true);
const Option<bool> PSAOptions::DumpICFGWrapper(
        "dump-icfgw",
        "Dump ICFG Wrapper",
        false);
const Option<bool> PSAOptions::PathSensitive(
        "path-sensitive",
        "Path sensitive analysis",
        true);

const Option<bool> PSAOptions::EnableLog(
        "log",
        "Enable logging",
        false);
const Option<bool> PSAOptions::EnableWarn(
        "warn",
        "Enable warning",
        false);
const Option<bool> PSAOptions::EnableReport(
        "report",
        "Enable bug report",
        false);
const Option<bool> PSAOptions::LoadAsUse(
        "load-as-use",
        "Use load statement as use",
        false);
const Option<bool> PSAOptions::EnableIsoSummary(
        "iso-summary",
        "Enable isolated summary",
        false);
const Option<bool> PSAOptions::EnableDataSlicing(
        "data-slicing",
        "Enable data slicing",
        true);
const Option<bool> PSAOptions::EnableExtCallSlicing(
        "ext-slicing",
        "Enable ext call slicing",
        false);
const Option<bool> PSAOptions::LEAK(
        "leak",
        "memory leak detector",
        false);
const Option<bool> PSAOptions::UAF(
        "uaf",
        "uaf detector",
        false);
const Option<bool> PSAOptions::DF(
        "df",
        "df detector",
        false);
const Option<bool> PSAOptions::Base(
        "base",
        "baseline solver and detector",
        false);
const Option<bool> PSAOptions::DumpState(
        "dump-stat",
        "dump stat to file",
        false);
const Option<bool> PSAOptions::Wrapper(
        "wrapper",
        "Use wrapper to identify FSM action",
        false);
const Option<u32_t> PSAOptions::MaxSrcLimit(
        "src-limit",
        "Maximum number of src points",
        5);
const Option<u32_t> PSAOptions::MaxSnkLimit(
        "snk-limit",
        "Maximum number of snk points",
        10);
const Option<u32_t> PSAOptions::LayerNum(
        "layer",
        "Maximum number of PDG layer",
        0);

const Option<u32_t> PSAOptions::MaxBoolNum(
        "max-bool",
        "Maximum number of bool branch conditions",
        30);

const Option<u32_t> PSAOptions::MaxSymbolSize(
        "max-symbol",
        "Maximum number of Z3Expr symbol",
        10);
const Option<u32_t> PSAOptions::MaxSQSize(
        "max-sq",
        "Maximum number of PI",
        100);
const Option<u32_t> PSAOptions::EvalNode(
        "eval",
        "Evaluated node",
        0);
const Option<u32_t> PSAOptions::MaxAddrs(
        "max-addrs",
        "Maximum addrs",
        20);
const Option<std::string> PSAOptions::LogLevel(
        "log-level",
        "print log level",
        "warning"
);

const Option<std::string> PSAOptions::FSMFILE("fsm-file",
                                        "path to the user-specified finite state machine",
                                        "res/memleak.fsm");

const Option<std::string> PSAOptions::OUTPUT("o",
                                             "output dir",
                                             "report/");

}


