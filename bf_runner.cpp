#include "bf_runner.h"

#include <unistd.h>

static std::string readCode(const std::string &filename)
{
    FILE *file = fopen(filename.c_str(), "rb");
    if (!file)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    std::string code(fileSize, '\0');
    size_t bytesRead = fread(&code[0], 1, fileSize, file);
    if (bytesRead != fileSize)
    {
        perror("Error reading file");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    fclose(file);
    return code;
}

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

void BfRunner::setSourcePath(const std::string &path)
{
    mSourcePath = path;
    mSourceCode = readCode(path);
}
