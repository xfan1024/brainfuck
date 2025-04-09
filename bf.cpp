#include <iostream>
#include <string>
#include <memory>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include "bf_runner.h"
#include "bf_runner_direct.h"
#include "bf_runner_bf_insn.h"
#include "bf_runner_jit.h"
#include "bf_compiler.h"

struct BfOption
{
    bool interpretBfInst;
    bool interpretDirect;
    bool interpretJit;
    bool aot;
    bool emitIr;
    bool printElapsed;
    bool noRun;
    std::string bfFile;
    std::string outputExecutablePath;
};

enum
{
    OPT_START = 0x100,
    OPT_BF_INSN,
    OPT_DIRECT,
    OPT_JIT,
    OPT_AOT,
    OPT_EMIT_IR,
    OPT_PRINT_ELAPSED,
};

BfOption parseOptions(int argc, char *argv[])
{
    BfOption options = {};
    struct option longOptions[] = {
        {"bf-insn", no_argument, nullptr, OPT_BF_INSN},
        {"direct", no_argument, nullptr, OPT_DIRECT},
        {"jit", no_argument, nullptr, OPT_JIT},
        {"aot", no_argument, nullptr, OPT_AOT},
        {"emit-ir", no_argument, nullptr, OPT_EMIT_IR},
        {"print-elapsed", no_argument, nullptr, OPT_PRINT_ELAPSED},
        {"no-run", no_argument, nullptr, 'n'},
        {"run", no_argument, nullptr, 'r'},
        {"output", required_argument, nullptr, 'o'},

        {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "nro:", longOptions, nullptr)) != -1)
    {
        switch (opt)
        {
            case OPT_BF_INSN:
                options.interpretBfInst = true;
                break;
            case OPT_DIRECT:
                options.interpretDirect = true;
                break;
            case OPT_JIT:
                options.interpretJit = true;
                break;
            case OPT_AOT:
                options.aot = true;
                options.noRun = true;
                break;
            case OPT_EMIT_IR:
                options.emitIr = true;
                break;
            case OPT_PRINT_ELAPSED:
                options.printElapsed = true;
                break;
            case 'n':
                options.noRun = true;
                break;
            case 'r':
                options.noRun = false;
                break;
            case 'o':
                options.outputExecutablePath = optarg;
                break;
            default:
                fprintf(stderr, "Unknown option: %c\n", optopt);
                exit(EXIT_FAILURE);
        }
    }

    {
        unsigned int modeCounter = 0;
        if (options.interpretBfInst) modeCounter++;
        if (options.interpretDirect) modeCounter++;
        if (options.interpretJit) modeCounter++;
        if (options.aot) modeCounter++;
        if (modeCounter > 1)
        {
            fprintf(stderr, "Error: Only one of --bf-insn, --direct, --jit, or --aot can be used.\n");
            exit(EXIT_FAILURE);
        }
        if (!modeCounter)
        {
            options.interpretBfInst = true;
        }
    }

    {
        if (options.interpretDirect && options.emitIr)
        {
            fprintf(stderr, "Error: --emit-ir cannot be used with --direct.\n");
            exit(EXIT_FAILURE);
        }
    }

    if (optind + 1 != argc)
    {
        fprintf(stderr, "Error: Expected one argument for bf file.\n");
        exit(EXIT_FAILURE);
    }

    options.bfFile = argv[optind];
    return options;
}


int main(int argc, char *argv[])
{
    BfOption options = parseOptions(argc, argv);
    std::shared_ptr<BfRunner> runner;

    if (options.interpretBfInst)
    {
        runner = std::make_shared<BfRunnerBfInsn>();
    }
    else if (options.interpretDirect)
    {
        runner = std::make_shared<BfRunnerDirect>();
    }
    else if (options.interpretJit)
    {
        runner = std::make_shared<BfRunnerJit>();
    }
    else if (options.aot)
    {
        if (options.outputExecutablePath.empty())
        {
            fprintf(stderr, "Error: --aot requires --output to be specified.\n");
            return 1;
        }
        BfCompiler *compiler = new BfCompiler();
        compiler->setOutputExecutablePath(options.outputExecutablePath);
        runner = std::shared_ptr<BfRunner>(compiler);
    }
    else
    {
        fprintf(stderr, "Error: No valid runner selected.\n");
        return 1;
    }

    runner->setEnableIrEmit(options.emitIr);

    runner->setSourcePath(options.bfFile);
    runner->preprocessCode();
    runner->compileCode();

    if (!options.noRun)
    {
        runner->run();
    }

    if (options.printElapsed)
    {
        auto &timer = runner->getElapsedTimer();
        timer.printElapsedTimes();
    }
    return 0;
}
