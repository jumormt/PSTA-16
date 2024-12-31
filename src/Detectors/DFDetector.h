//
// Created by Jiawei Ren on 2022/2/28.
//

#ifndef PSA_DFDETECTOR_H
#define PSA_DFDETECTOR_H

#include "PSTA/PSTA.h"

namespace SVF {
/*!
 * Double free detector
 */
class DFDetector final : public PSTA {
public:
    enum DF_TYPE {
        SAFE = 0,
        DF_ERROR
    };

    /// Constructor
    explicit DFDetector() = default;

    /// Destructor
    ~DFDetector() override = default;

    /// We start from here
    bool runFSMOnModule(SVFModule *module) override;

    void initHandler(SVFModule *module) override;


protected:
    void reportDF(const ICFGNode *node, const Z3Expr &branchCond, const KeyNodesSet &keyNodesSet);

    void reportBug() override;

    void validateSuccessTests(DF_TYPE uafType, const ICFGNode *node);

    void validateExpectedFailureTests(DF_TYPE uafType, const ICFGNode *node);
};
}


#endif //PSA_DFDETECTOR_H
