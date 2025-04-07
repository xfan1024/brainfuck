#ifndef __BF_RUNNER_BF_INST_H__
#define __BF_RUNNER_BF_INST_H__

#include "bf_runner.h"
#include <vector>
#include <stdint.h>

enum
{
    BF_INSN_AA,
    BF_INSN_VA,
    BF_INSN_VI,
    BF_INSN_VO,
    BF_INSN_LB,
    BF_INSN_LE,
    BF_INSN_MAX,
};
struct bf_insn
{
    int32_t opcode;
    int32_t operand;
};

class BfRunnerBfInsn : public BfRunner
{
public:
    BfRunnerBfInsn() = default;
    ~BfRunnerBfInsn() override = default;

    void compileCode() override;
    void run() override;
private:

    std::vector<bf_insn> mInsns;
};


#endif // __BF_RUNNER_BF_INST_H__
