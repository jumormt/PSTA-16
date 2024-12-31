//
// Created by Jiawei Ren on 2023/7/2.
//

#ifndef TYPESTATE_UAFDETECTORBASE_H
#define TYPESTATE_UAFDETECTORBASE_H


#include "PSTA/PSTABase.h"

namespace SVF {
/*!
 * Use after free detector Base
 */
    class UAFDetectorBase final : public PSTABase {
    public:
        enum UAF_TYPE {
            SAFE = 0,
            UAF_ERROR
        };

        /// Constructor
        explicit UAFDetectorBase() = default;

        /// Destructor
        ~UAFDetectorBase() override = default;

        /// We start from here
        bool runFSMOnModule(SVFModule *module) override;

        void initHandler(SVFModule *module) override;

    protected:
        void reportUAF(const ICFGNode *node, const Z3Expr &branchCond, const KeyNodesSet &keyNodesSet);

        void reportBug() override;

        void validateSuccessTests(UAF_TYPE uafType, const ICFGNode *node);

        void validateExpectedFailureTests(UAF_TYPE uafType, const ICFGNode *node);

    };
}


#endif //TYPESTATE_UAFDETECTORBASE_H
