#include <vector>
#include <string>
#include <sstream>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

#define BF_MEM_SIZE (1 << 20)
#define BF_ADDR_MASK (BF_MEM_SIZE - 1)
#define BF_INSN_SIZE (1 << 20)

// brainfuck 分为8种指令：{'.', ',', '<', '>', '+', '-', '[', ']'}
// 为了方便处理，在解析阶段将指令转为bf_insn结构体，并且insn支持如下指令
//  AA  (addr add)      "<>" 合并，例如 ">>>" 合并为 "AA 3", "<<<" 合并为 "AA -3"
//  VA  (value add)     "++" 合并，例如 "+++" 合并为 "VA 3", "---" 合并为 "VA -3"
//  VI  (value in)      "," 不合并，操作数无意义
//  VO  (value out)     "." 不合并，操作数无意义
//  LB  (loop begin)    "[" 不合并，操作数为对应LE指令的地址
//  LE  (loop end)      "]" 不合并，操作数为对应LB指令的地址

enum
{
    BF_INSN_AA,
    BF_INSN_VA,
    BF_INSN_VI,
    BF_INSN_VO,
    BF_INSN_LB,
    BF_INSN_LE,
    BF_INSN_MAX,
};

struct bf_insn
{
    int32_t opcode;
    int32_t operand;
};

struct bf_fd
{
    char *filename;
    int fd;
    bool needs_close;
};

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

std::string bfcode_read(const char *filename)
{
    std::string code;
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        perror("Error opening file");
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    code.resize(size);
    fread(&code[0], 1, size, fp);
    fclose(fp);
    return code;
}

void bf_insn_dump(const std::vector<bf_insn> &insns)
{
    const char *bf_insn_name[BF_INSN_MAX] =
    {
        "AA",
        "VA",
        "VI",
        "VO",
        "LB",
        "LE",
    };

    for (size_t i = 0; i < insns.size(); i++)
    {
        int32_t code = insns[i].opcode;
        int32_t operand = insns[i].operand;
        const char *name = bf_insn_name[code];
        printf("%-8zu %s %d\n", i, name, operand);
    }
}

void bf_program_run(const std::vector<bf_insn> &insns)
{
    unsigned int pc = 0;
    unsigned int addr = 0;
    std::vector<uint8_t> memory(BF_MEM_SIZE, 0);

    while (pc < insns.size())
    {
        // printf("pc: %u, addr: %u\n", pc, addr);
        const bf_insn &insn = insns[pc];
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
                    uint8_t c;
                    if (read(STDIN_FILENO, &c, 1) == 1)
                    {
                        memory[addr] = c;
                    }
                    else
                    {
                        memory[addr] = 0;
                    }
                }
                break;
            case BF_INSN_VO:
                {
                    write(STDOUT_FILENO, &memory[addr], 1);
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
}

uint64_t timespec_diff(const struct timespec *a, const struct timespec *b)
{
    uint64_t res;
    if (a->tv_nsec < b->tv_nsec)
    {
        res = 1000000000 + a->tv_nsec - b->tv_nsec;
        res += 1000000000 * (a->tv_sec - b->tv_sec - 1);
    }
    else
    {
        res = a->tv_nsec - b->tv_nsec;
        res += 1000000000 * (a->tv_sec - b->tv_sec);
    }
    return res;
}

unsigned int bf_program_run_measure_elapsed(const std::vector<bf_insn> &insns)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    bf_program_run(insns);
    clock_gettime(CLOCK_MONOTONIC, &end);
    uint64_t elapsed = timespec_diff(&end, &start);
    return (unsigned int)(elapsed / 1000);
}


int main(int argc, char *argv[])
{
    const char *bfcode_file = NULL;
    const char *input_file = NULL;
    const char *output_file = NULL;
    if (argc > 1)
    {
        bfcode_file = argv[1];
    }
    else
    {
        fprintf(stderr, "Usage: %s <bf file> [input [output]]\n", argv[0]);
        return 1;
    }
    if (argc > 2)
    {
        input_file = argv[2];
    }
    if (argc > 3)
    {
        output_file = argv[3];
    }

    std::string bfcode = bfcode_read(bfcode_file);
    std::vector<bf_insn> insns = bf_insn_parse(bfcode.c_str());
    
    // bf_insn_dump(insns);
    
    unsigned int elapsed_ms =bf_program_run_measure_elapsed(insns);
    printf("Execution time: %u us\n", elapsed_ms);

    return 0;
}
