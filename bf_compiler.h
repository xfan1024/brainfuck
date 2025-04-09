#ifndef __bf_compiler_h__
#define __bf_compiler_h__

#include "bf_runner_bf_insn.h"
#include <string>

class BfCompiler : public BfRunnerBfInsn
{
public:
    BfCompiler() = default;
    ~BfCompiler() override = default;

    void setOutputExecutablePath(const std::string &path)
    {
        mOutputExecutablePath = path;
    }
    void compileCode() override;
    void run() override;
private:
    std::string mOutputExecutablePath;
};

#endif // __bf_compiler_h__
