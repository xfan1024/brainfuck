#include "bf_runner.h"

#include <unistd.h>

void BfRunner::preprocessCode()
{
    // TODO: remove comments and space
}

void BfRunner::writeByte(BfRunner *runner, unsigned char byte)
{
    write(runner->mOutputFd, &byte, 1);    
}

unsigned char BfRunner::readByte(BfRunner *runner)
{
    unsigned char byte;
    if (read(runner->mInputFd, &byte, 1) != 1)
    {
        // EOF?
        return 0;
    }
    return byte;
}
