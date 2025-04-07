#include <iostream>
#include <string>
#include <memory>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include "bf_runner.h"
#include "bf_runner_direct.h"
#include "bf_runner_bf_insn.h"

struct BfOption
{
    bool interpretBfInst;
    bool interpretDirect;
    bool interpretAot;
    bool emitIr;
    bool logElapsedTime;
    std::string bfFile;
};

enum
{
    OPT_START = 0x100,
    OPT_BF_INST,
    OPT_DIRECT,
    OPT_AOT,
    OPT_EMIT_IR,
    OPT_LOG_ELAPSED_TIME,
};

BfOption parseOptions(int argc, char *argv[])
{
    BfOption options = {};
    struct option longOptions[] = {
        {"bf-inst", no_argument, nullptr, OPT_BF_INST},
        {"direct", no_argument, nullptr, OPT_DIRECT},
        {"aot", no_argument, nullptr, OPT_AOT},
        {"emit-ir", no_argument, nullptr, OPT_EMIT_IR},
        {"log-elapsed-time", no_argument, nullptr, OPT_LOG_ELAPSED_TIME},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "", longOptions, nullptr)) != -1)
    {
        switch (opt)
        {
            case OPT_BF_INST:
                options.interpretBfInst = true;
                break;
            case OPT_DIRECT:
                options.interpretDirect = true;
                break;
            case OPT_AOT:
                options.interpretAot = true;
                break;
            case OPT_EMIT_IR:
                options.emitIr = true;
                break;
            case OPT_LOG_ELAPSED_TIME:
                options.logElapsedTime = true;
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
        if (options.interpretAot) modeCounter++;
        if (modeCounter > 1)
        {
            fprintf(stderr, "Error: Only one of --bf-inst, --direct, or --aot can be specified.\n");
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

std::string readCode(const std::string &filename)
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
    else if (options.interpretAot)
    {
        // runner = std::make_shared<BfAotRunner>(options.bfFile);
        fprintf(stderr, "NOTE: --aot is not implemented yet.\n");
        exit(EXIT_FAILURE);
    }

    std::string code = readCode(options.bfFile);
    runner->setCode(code);
    runner->preprocessCode();
    runner->compileCode();
    runner->run();

    if (options.logElapsedTime)
    {
        auto &timer = runner->getElapsedTimer();
        timer.printElapsedTimes();
    }
    return 0;
}
