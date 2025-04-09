#ifndef __bf_runner_h__
#define __bf_runner_h__

#include <memory>
#include <string>
#include <unistd.h>
#include "bf_elapsed_timer.h"

#define BF_MEM_SIZE (1 << 20)
#define BF_ADDR_MASK (BF_MEM_SIZE - 1)

class BfRunner
{
public:
    BfRunner() = default;
    virtual ~BfRunner() = default;
    
    void setInputFd(int fd)
    {
        mInputFd = fd;
    }

    void setOutputFd(int fd)
    {
        mOutputFd = fd;
    }

    void setEnableIrEmit(bool enable)
    {
        mEnableIrEmit = enable;
    }

    void setSourcePath(const std::string &path);

    BfElapsedTimer &getElapsedTimer()
    {
        return mTimer;
    }

    virtual void preprocessCode();
    virtual void compileCode() = 0;
    virtual void run() = 0;

protected:
    BfElapsedTimer mTimer;
    std::string mSourceCode;
    std::string mSourcePath;
    bool mEnableIrEmit {false};
    static void writeByte(BfRunner *runner, unsigned char byte);
    static unsigned char readByte(BfRunner *runner);

private:
    int mInputFd {STDIN_FILENO};
    int mOutputFd {STDOUT_FILENO};
};

#endif
