#ifndef __BF_RUNNER_DIRECT_H__
#define __BF_RUNNER_DIRECT_H__

#include "bf_runner.h"

class BfRunnerDirect : public BfRunner
{
public:
    BfRunnerDirect() = default;
    ~BfRunnerDirect() override = default;

    void compileCode() override {}
    struct BfRunnerDirectContext;
    void run() override;
private:
    void step(BfRunnerDirectContext *brdc);
};

#endif // __BF_RUNNER_DIRECT_H__
