//
// Created by Jiawei Ren on 2023/7/2.
//

#ifndef TYPESTATE_MEMLEAKDETECTORBASE_H
#define TYPESTATE_MEMLEAKDETECTORBASE_H


#include "PSTA/PSTABase.h"


namespace SVF {

/*!
 * Memory leak detector Base
 */
    class MemLeakDetectorBase final : public PSTABase {

    public:
        enum LEAK_TYPE {
            SAFE = 0,
            NEVER_FREE_LEAK,
            CONTEXT_LEAK,
            PATH_LEAK,
            GLOBAL_LEAK
        };

        /// Constructor
        explicit MemLeakDetectorBase() = default;

        /// Destructor
        ~MemLeakDetectorBase() override = default;

        /// We start from here
        bool runFSMOnModule(SVFModule *module) override;

        void initHandler(SVFModule *module) override;


    protected:
        void reportNeverFree(const Z3Expr &branchCond, const KeyNodesSet &keyNodesSet);

        void reportPartialLeak(const Z3Expr &branchCond, const KeyNodesSet &keyNodesSet);

        void reportBug() override;

        void validateSuccessTests(LEAK_TYPE leakType);

        void validateExpectedFailureTests(LEAK_TYPE leakType);


    };
}


#endif //TYPESTATE_MEMLEAKDETECTORBASE_H
