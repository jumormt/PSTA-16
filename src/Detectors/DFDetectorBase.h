//
// Created by Jiawei Ren on 2023/7/2.
//

#ifndef TYPESTATE_DFDETECTORBASE_H
#define TYPESTATE_DFDETECTORBASE_H

#include "PSTA/PSTABase.h"
namespace SVF {
/*!
 * Double free detector Base
 */
    class DFDetectorBase final : public PSTABase {
    public:
        enum DF_TYPE {
            SAFE = 0,
            DF_ERROR
        };

        /// Constructor
        explicit DFDetectorBase() = default;

        /// Destructor
        ~DFDetectorBase() override = default;

        /// We start from here
        bool runFSMOnModule(SVFModule *module) override;

        void initHandler(SVFModule *module) override;


    protected:
        void reportDF(const ICFGNode *node);

        void reportBug() override;

        void validateSuccessTests(DF_TYPE uafType, const ICFGNode *node);

        void validateExpectedFailureTests(DF_TYPE uafType, const ICFGNode *node);
    };
}


#endif //TYPESTATE_DFDETECTORBASE_H
