#ifndef BF_RUNNER_JIT_H
#define BF_RUNNER_JIT_H

#include "bf_runner_bf_insn.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"

class BfRunnerJit : public BfRunnerBfInsn
{
public:
    BfRunnerJit();
    ~BfRunnerJit() override = default;

    void compileCode() override;
    void run() override;
private:
    llvm::LLVMContext mContext;
    std::unique_ptr<llvm::orc::LLJIT> mJIT;
};

#endif // BF_RUNNER_JIT_H
