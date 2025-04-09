#include "bf_runner_bf_insn.h"

void bf_insn_append(std::vector<bf_insn> &insns, int32_t code, int32_t operand)
{
    insns.push_back({code, operand});
}

std::vector<bf_insn> bf_insn_parse(const char *code)
{
    std::vector<bf_insn> insns;
    std::vector<int32_t> le_positions;
    char c;

    while ((c = *code))
    {
        switch (c)
        {
            case '>':
            case '<':
            {
                int32_t dir = (c == '>') ? 1 : -1;
                if (!insns.empty() && insns.back().opcode == BF_INSN_AA)
                {
                    insns.back().operand += dir;
                    if (insns.back().operand == 0)
                    {
                        insns.pop_back();
                    }
                }
                else
                {
                    bf_insn_append(insns, BF_INSN_AA, dir);
                }
                code++;
                break;
            }

            case '+':
            case '-':
            {
                int32_t dir = (c == '+') ? 1 : -1;
                if (!insns.empty() && insns.back().opcode == BF_INSN_VA)
                {
                    insns.back().operand += dir;
                    if (insns.back().operand == 0)
                    {
                        insns.pop_back();
                    }
                }
                else
                {
                    bf_insn_append(insns, BF_INSN_VA, dir);
                }
                code++;
                break;
            }

            case '.':
            {
                bf_insn_append(insns, BF_INSN_VO, 0);
                code++;
                break;
            }
            case ',':
            {
                bf_insn_append(insns, BF_INSN_VI, 0);
                code++;
                break;
            }
            case '[':
            {
                int32_t le_pos = (int32_t)insns.size();
                bf_insn_append(insns, BF_INSN_LB, 0); // fix operand later
                le_positions.push_back(le_pos);
                code++;
                break;
            }
            case ']':
            {
                if (le_positions.empty())
                {
                    fprintf(stderr, "Error: Unmatched ']' in source code\n");
                    exit(1);
                }
                int32_t lb_pos = le_positions.back();
                int32_t le_pos = (int32_t)insns.size();
                le_positions.pop_back();

                insns[lb_pos].operand = le_pos; // fix operand
                bf_insn_append(insns, BF_INSN_LE, lb_pos);
                code++;
                break;
            }
            default:
            {
                if (isspace(c))
                {
                    code++;
                }
                else
                {
                    fprintf(stderr, "Error: Invalid instruction '%c' in source code\n", c);
                    exit(1);
                }
                break;
            }
        }
    }
    return insns;
}

static void dumpBfInsnToFile(const std::string &bf_file, std::vector<bf_insn> &insns)
{
    const char *bfInsnName[BF_INSN_MAX] =
    {
        "AA",
        "VA",
        "VI",
        "VO",
        "LB",
        "LE",
    };

    std::string ir_file = bf_file + ".bfinsn";
    FILE *fp = fopen(ir_file.c_str(), "w");
    if (!fp)
    {
        fprintf(stderr, "Error: Cannot open file %s\n", ir_file.c_str());
        return;
    }

    for (unsigned int i = 0; i < insns.size(); i++)
    {
        int32_t code = insns[i].opcode;
        int32_t operand = insns[i].operand;
        const char *name = bfInsnName[code];
        if (code == BF_INSN_VI || code == BF_INSN_VO)
        {
            fprintf(fp, "[%05x] %s\n", i, name);
        }
        else if (code == BF_INSN_LB || code == BF_INSN_LE)
        {
            fprintf(fp, "[%05x] %s %05x\n", i, name, (unsigned int)operand);
        }
        else
        {
            fprintf(fp, "[%05x] %s %d\n", i, name, (int)operand);
        }
    }
    fclose(fp);
}


void BfRunnerBfInsn::compileCode()
{
    mTimer.start("GEN bf-insn");
    mInsns = bf_insn_parse(mSourceCode.c_str());
    mTimer.stop();

    if (mEnableIrEmit)
    {
        dumpBfInsnToFile(mSourcePath, mInsns);
    }
}

void BfRunnerBfInsn::run()
{
    mTimer.start("RUN bf-insn");

    unsigned int pc = 0;
    unsigned int addr = 0;
    std::vector<uint8_t> memory(BF_MEM_SIZE, 0);

    while (pc < mInsns.size())
    {
        // printf("pc: %u, addr: %u\n", pc, addr);
        const bf_insn &insn = mInsns[pc];
        switch (insn.opcode)
        {
            case BF_INSN_AA:
                addr = (addr + insn.operand) & BF_ADDR_MASK;
                break;
            case BF_INSN_VA:
                memory[addr] += insn.operand;
                break;
            case BF_INSN_VI:
                {
                    memory[addr] = readByte(this);
                }
                break;
            case BF_INSN_VO:
                {
                    writeByte(this, memory[addr]);
                }
                break;
            case BF_INSN_LB:
                if (memory[addr] == 0)
                {
                    pc = insn.operand;
                }
                break;
            case BF_INSN_LE:
                if (memory[addr] != 0)
                {
                    pc = insn.operand;
                }
                break;
            default:
                fprintf(stderr, "Error: Invalid instruction %d\n", insn.opcode);
                exit(1);
        }
        pc++;
    }

    mTimer.stop();
}
