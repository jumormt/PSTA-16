//===- svf-ex.cpp -- A driver example of SVF-------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===-----------------------------------------------------------------------===//

/*
 // A driver program of SVF including usages of SVF APIs
 //
 // Author: Yulei Sui,
 */

#include "SVF-LLVM/LLVMUtil.h"
#include "Detectors/MemLeakDetector.h"
#include "Detectors/UAFDetector.h"
#include "Detectors/DFDetector.h"
#include "Detectors/MemLeakDetectorBase.h"
#include "Detectors/UAFDetectorBase.h"
#include "Detectors/DFDetectorBase.h"
#include "PSTA/PSTABase.h"

using namespace SVF;

int main(int argc, char **argv) {
    // add arguments for svf
    int arg_num = 0;
    int extraArgc = 5;
    char **arg_value = new char *[argc + extraArgc];
    for (; arg_num < argc; ++arg_num) {
        arg_value[arg_num] = argv[arg_num];
    }
    int orgArgNum = arg_num;
    arg_value[arg_num++] = (char *) "-model-consts=true";
    arg_value[arg_num++] = (char *) "-model-arrays=true";
    arg_value[arg_num++] = (char *) "-pre-field-sensitive=false";
    arg_value[arg_num++] = (char *) "-ff-eq-base";
    arg_value[arg_num++] = (char *) "-field-limit=16";
    assert(arg_num == (orgArgNum + extraArgc) && "more extra arguments? Change the value of extraArgc");

    // build svf module
    std::vector<std::string> moduleNameVec;
    moduleNameVec = OptionBase::parseOptions(
            arg_num, arg_value, "Path-sensitive Typestate Analysis", "[options] <input-bitcode...>"
    );
    SVFModule *svfModule = LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(moduleNameVec);
    std::unique_ptr<PSTABase> detector = nullptr;
    // create detector
    if (PSAOptions::Base())
    {
        if (PSAOptions::LEAK()) {
            detector = std::make_unique<MemLeakDetectorBase>();
        } else if (PSAOptions::UAF()) {
            detector = std::make_unique<UAFDetectorBase>();
        } else if (PSAOptions::DF()) {
            detector = std::make_unique<DFDetectorBase>();
        } else {
            assert(false && "invalid detector!");
        }
        // start analysis
        detector->runFSMOnModule(svfModule);
    } else {
        if (PSAOptions::LEAK()) {
            detector = std::make_unique<MemLeakDetector>();
        } else if (PSAOptions::UAF()) {
            detector = std::make_unique<UAFDetector>();
        } else if (PSAOptions::DF()) {
            detector = std::make_unique<DFDetector>();
        } else {
            assert(false && "invalid detector!");
        }
        // start analysis
        detector->runFSMOnModule(svfModule);
    }

    delete[] arg_value;
    return 0;
}

