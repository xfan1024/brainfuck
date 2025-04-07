#include "bf_runner_direct.h"
#include <stdint.h>

#define BF_MEM_SIZE (1 << 20)
#define BF_ADDR_MASK (BF_MEM_SIZE - 1)

struct BfRunnerDirect::BfRunnerDirectContext
{
    uint8_t *memoryBase;
    uint32_t memoryAddr;
    uint32_t nestedCount;
    const char *codePtr;
};

void BfRunnerDirect::step(BfRunnerDirectContext *brdc)
{
    char inst = *brdc->codePtr;
    switch (inst)
    {
    case '>':
        brdc->memoryAddr = (brdc->memoryAddr + 1) & BF_ADDR_MASK;
        break;
    case '<':
        brdc->memoryAddr = (brdc->memoryAddr - 1) & BF_ADDR_MASK;
        break;
    case '+':
        brdc->memoryBase[brdc->memoryAddr]++;
        break;
    case '-':
        brdc->memoryBase[brdc->memoryAddr]--;
        break;
    case '.':
        writeByte(this, brdc->memoryBase[brdc->memoryAddr]);
        break;
    case ',':
        brdc->memoryBase[brdc->memoryAddr] = readByte(this);
        break;
    case '[':
        if (brdc->memoryBase[brdc->memoryAddr] == 0)
        {
            int bracket_count = 1;
            while (bracket_count > 0 && *++brdc->codePtr != '\0')
            {
                if (*brdc->codePtr == '[')
                {
                    bracket_count++;
                }
                else if (*brdc->codePtr == ']')
                {
                    bracket_count--;
                }
            }
            if (bracket_count > 0)
            {
                fprintf(stderr, "Error: Unmatched '[' in source code\n");
                exit(1);
            }
        }
        else
        {
            brdc->nestedCount++;
        }
        break;
    case ']':
        if (brdc->nestedCount == 0)
        {
            fprintf(stderr, "Error: Unmatched ']' in source code\n");
            exit(1);
        }
        if (brdc->memoryBase[brdc->memoryAddr] != 0)
        {
            int bracket_count = 1;
            while (bracket_count > 0)
            {
                brdc->codePtr--;
                if (*brdc->codePtr == '[')
                {
                    bracket_count--;
                }
                else if (*brdc->codePtr == ']')
                {
                    bracket_count++;
                }
            }
        }
        else
        {
            brdc->nestedCount--;
        }
        break;
    case '\0':
        fprintf(stderr, "Error: End of code reached\n");
        exit(1);
    default:
        if (!isspace(inst))
        {
            if (isprint(inst))
            {
                fprintf(stderr, "Error: Invalid instruction '%c' in source code\n", inst);
            }
            else
            {
                fprintf(stderr, "Error: Invalid instruction (0x%02x) in source code\n", inst);
            }
            exit(1);
        }
        break;
    }
}



void BfRunnerDirect::run()
{
    std::vector<uint8_t> memoryBase(BF_MEM_SIZE, 0);

    BfRunnerDirectContext brdc = {0};
    brdc.memoryBase = memoryBase.data();
    brdc.codePtr = mSourceCode.c_str();

    mTimer.start("interpret");
    while (*brdc.codePtr != '\0')
    {
        step(&brdc);
        brdc.codePtr++;
    }
    mTimer.stop();
}
