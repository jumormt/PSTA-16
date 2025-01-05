//
// Created by Xiao on 2022/1/2.
//

#ifndef PSA_MEMLEAKDETECTOR_H
#define PSA_MEMLEAKDETECTOR_H

#include "PSTA/PSTA.h"


namespace SVF {

/*!
 * Memory leak detector
 */
class MemLeakDetector final : public PSTA {

public:
    enum LEAK_TYPE {
        SAFE = 0,
        NEVER_FREE_LEAK,
        CONTEXT_LEAK,
        PATH_LEAK,
        GLOBAL_LEAK
    };

    /// Constructor
    explicit MemLeakDetector() = default;

    /// Destructor
    ~MemLeakDetector() override = default;

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


#endif //PSA_MEMLEAKDETECTOR_H
