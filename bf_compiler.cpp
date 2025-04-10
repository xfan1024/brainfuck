#include "bf_compiler.h"
#include "bf_llvm_ir.h"
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

static const std::string mainCode = R"(
#include <stdio.h>
#include <stdint.h>

#define MEM_SIZE (1 << 20)
static uint8_t memory[MEM_SIZE];

typedef uint8_t (*readbyte_func)(void *);
typedef void (*writebyte_func)(void *, char);

extern void bfcode(void *ctx, void *memory, readbyte_func readbyte, writebyte_func writebyte);

static void bf_writebyte(void *ctx, char c)
{
    putchar(c);
}

static uint8_t bf_readbyte(void *ctx)
{
    int ch = getchar();
    if (ch == EOF)
    {
        return 0;
    }
    return (uint8_t)ch;
}

int main()
{
    void *ctx = NULL;
    bfcode(ctx, memory, bf_readbyte, bf_writebyte);
    return 0;
}
)";

static void clangCompile(const std::string main, const std::string &llvmIr, const std::string outputPath)
{
    const char *cc, *cflags;
    char mainTemplate[] = "/tmp/mainXXXXXX.c"; // 后缀 ".c" 长度为2
    char llvmTemplate[] = "/tmp/llvmXXXXXX.ll"; // 后缀 ".ll" 长度为3
    std::string clangCmd;
    int ret = 0;

    int mainFd = -1;
    int llvmFd = -1;

    if ((mainFd = mkostemps(mainTemplate, 2, O_RDWR)) == -1)
    {
        perror("mkstemp");
        goto err;
    }

    if ((llvmFd = mkostemps(llvmTemplate, 3, O_RDWR)) == -1)
    {
        perror("mkstemp");
        goto err;
    }

    write(mainFd, main.c_str(), main.size());
    write(llvmFd, llvmIr.c_str(), llvmIr.size());
    close(mainFd);
    close(llvmFd);
    mainFd = -1;
    llvmFd = -1;

    cc = getenv("CC");
    cflags = getenv("CFLAGS");

    if (cc == NULL)
    {
        cc = "clang";
    }
    if (cflags == NULL)
    {
        cflags = "";
    }

    clangCmd = std::string(cc) + " " + cflags + " " + llvmTemplate + " " + mainTemplate + " -o " + outputPath;

    ret = system(clangCmd.c_str());
    unlink(mainTemplate);
    unlink(llvmTemplate);

    if (ret != 0)
    {
        fprintf(stderr, "Error: clang command failed with code %d\n", ret);
        exit(1);
    }

    return;
err:
    if (mainFd != -1)
    {
        close(mainFd);
        unlink(mainTemplate);
    }
    if (llvmFd != -1)
    {
        close(llvmFd);
        unlink(llvmTemplate);
    }
    exit(1);
}

void BfCompiler::compileCode()
{
    BfRunnerBfInsn::compileCode();

    mTimer.start("GEN llvm-ir");
    std::string llvmIr = bfLlvmIrGenerate(mInsns);
    mTimer.stop();

    if (mEnableIrEmit)
    {
        bfLlvmIrDumpToFile(mSourcePath, llvmIr);
    }

    mTimer.start("GEN exe");
    if (mOutputExecutablePath.empty())
    {
        fprintf(stderr, "Error: Output executable path is not set\n");
        exit(1);
    }
    clangCompile(mainCode, llvmIr, mOutputExecutablePath);
    mTimer.stop();
}

void BfCompiler::run()
{
    if (mOutputExecutablePath.empty())
    {
        fprintf(stderr, "Error: Output executable path is not set\n");
        exit(1);
    }

    const char *exeArgv[] = {mOutputExecutablePath.c_str(), NULL};

    mTimer.start("RUN exe");
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        exit(1);
    }

    if (pid == 0)
    {
        // Child process
        execv(mOutputExecutablePath.c_str(), (char **)exeArgv);
        perror("execv");
        exit(1);
    }
    else
    {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        WIFEXITED(status);
    }

    mTimer.stop();
}
