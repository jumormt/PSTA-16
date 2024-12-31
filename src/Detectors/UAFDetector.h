//
// Created by Jiawei Ren on 2022/2/26.
//

#ifndef PSA_UAFDETECTOR_H
#define PSA_UAFDETECTOR_H

#include "PSTA/PSTA.h"

namespace SVF {
/*!
 * Use after free detector
 */
class UAFDetector final : public PSTA {
public:
    enum UAF_TYPE {
        SAFE = 0,
        UAF_ERROR
    };

    /// Constructor
    explicit UAFDetector() = default;

    /// Destructor
    ~UAFDetector() override = default;

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


#endif //PSA_UAFDETECTOR_H
