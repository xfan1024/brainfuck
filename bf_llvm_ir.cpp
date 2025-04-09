
#include "bf_llvm_ir.h"
#include <sstream>
#include <stdio.h>

static void bf_llvm_ir_emit_va(std::ostream &os, unsigned int ninst, int operand)
{
    os << "inst" << ninst << ":\n";
    os << "  ; VA " << operand << "\n";
    os << "  %inst" << ninst << "_addr = load i32, i32* %addr\n";
    os << "  %inst" << ninst << "_mem = getelementptr i8, i8* %memory, i32 %inst" << ninst << "_addr\n";
    os << "  %inst" << ninst << "_val = load i8, i8* %inst" << ninst << "_mem\n";
    os << "  %inst" << ninst << "_val2 = add i8 %inst" << ninst << "_val, " << operand << "\n";
    os << "  store i8 %inst" << ninst << "_val2, i8* %inst" << ninst << "_mem\n";
    os << "  br label %inst" << (ninst + 1) << "\n";
}

static void bf_llvm_ir_emit_aa(std::ostream &os, unsigned int ninst, int operand)
{
    os << "inst" << ninst << ":\n";
    os << "  ; AA " << operand << "\n";
    os << "  %inst" << ninst << "_addr = load i32, i32* %addr\n";
    os << "  %inst" << ninst << "_addr2 = add i32 %inst" << ninst << "_addr, " << operand << "\n";
    os << "  %inst" << ninst << "_addr3 = and i32 %inst" << ninst << "_addr2, " << BF_ADDR_MASK << "\n";
    os << "  store i32 %inst" << ninst << "_addr3, i32* %addr\n";
    os << "  br label %inst" << (ninst + 1) << "\n";
}

static void bf_llvm_ir_emit_lb(std::ostream &os, unsigned int ninst, int operand)
{
    os << "inst" << ninst << ":\n";
    os << "  ; LB " << operand << "\n";
    os << "  %inst" << ninst << "_addr = load i32, i32* %addr\n";
    os << "  %inst" << ninst << "_mem = getelementptr i8, i8* %memory, i32 %inst" << ninst << "_addr\n";
    os << "  %inst" << ninst << "_val = load i8, i8* %inst" << ninst << "_mem\n";
    os << "  %inst" << ninst << "_cond = icmp eq i8 %inst" << ninst << "_val, 0\n";
    os << "  br i1 %inst" << ninst << "_cond, label %inst" << (operand + 1) << ", label %inst" << (ninst + 1) << "\n";
}

static void bf_llvm_ir_emit_le(std::ostream &os, unsigned int ninst, int operand)
{
    os << "inst" << ninst << ":\n";
    os << "  ; LE " << operand << "\n";
    os << "  br label %inst" << operand << "\n";
}

static void bf_llvm_ir_emit_vi(std::ostream &os, unsigned int ninst)
{
    os << "inst" << ninst << ":\n";
    os << "  ; VI\n";
    os << "  %inst" << ninst << "_addr = load i32, i32* %addr\n";
    os << "  %inst" << ninst << "_mem = getelementptr i8, i8* %memory, i32 %inst" << ninst << "_addr\n";
    os << "  %inst" << ninst << "_val = call i8 %readbyte(i8* %ctx)\n";
    os << "  store i8 %inst" << ninst << "_val, i8* %inst" << ninst << "_mem\n";
    os << "  br label %inst" << (ninst + 1) << "\n";
}

static void bf_llvm_ir_emit_vo(std::ostream &os, unsigned int ninst)
{
    os << "inst" << ninst << ":\n";
    os << "  ; VO\n";
    os << "  %inst" << ninst << "_addr = load i32, i32* %addr\n";
    os << "  %inst" << ninst << "_mem = getelementptr i8, i8* %memory, i32 %inst" << ninst << "_addr\n";
    os << "  %inst" << ninst << "_val = load i8, i8* %inst" << ninst << "_mem\n";
    os << "  call void %writebyte(i8* %ctx, i8 %inst" << ninst << "_val)\n";
    os << "  br label %inst" << (ninst + 1) << "\n";
}

static void bf_llvm_ir_emit_inst(std::ostream &os, unsigned int ninst, const bf_insn &insn)
{
    switch (insn.opcode)
    {
        case BF_INSN_AA:
            bf_llvm_ir_emit_aa(os, ninst, insn.operand);
            break;
        case BF_INSN_VA:
            bf_llvm_ir_emit_va(os, ninst, insn.operand);
            break;
        case BF_INSN_LB:
            bf_llvm_ir_emit_lb(os, ninst, insn.operand);
            break;
        case BF_INSN_LE:
            bf_llvm_ir_emit_le(os, ninst, insn.operand);
            break;
        case BF_INSN_VI:
            bf_llvm_ir_emit_vi(os, ninst);
            break;
        case BF_INSN_VO:
            bf_llvm_ir_emit_vo(os, ninst);
            break;
        default:
            fprintf(stderr, "Error: Invalid instruction %d\n", insn.opcode);
            exit(1);
    }
}

static void bf_llvm_ir_emit_header(std::ostream &os, size_t inst_count)
{
    os << "define void @bfcode(i8* %ctx, i8* %memory, i8 (i8*)* %readbyte, void (i8*, i8)* %writebyte)\n";
    os << "{\n";
    os << "entry:\n";
    os << "  %addr = alloca i32\n";
    os << "  store i32 0, i32* %addr\n";
    os << "  br label %inst0\n";
}

static void bf_llvm_ir_emit_footer(std::ostream &os, size_t inst_count)
{
    os << "inst" << inst_count << ":\n";
    os << "  ; end\n";
    os << "  ret void\n";
    os << "}\n";
}

void bfLlvmIrDumpToFile(const std::string &bf_file, const std::string &llvm_ir)
{
    std::string ir_file = bf_file + ".ll";
    FILE *fp = fopen(ir_file.c_str(), "w");
    if (!fp)
    {
        fprintf(stderr, "Error: Cannot open file %s\n", ir_file.c_str());
        return;
    }

    fprintf(fp, "%s", llvm_ir.c_str());
    fclose(fp);
}

std::string bfLlvmIrGenerate(const std::vector<bf_insn> &insns)
{
    std::ostringstream os;
    bf_llvm_ir_emit_header(os, insns.size());
    for (unsigned int i = 0; i < insns.size(); i++)
    {
        bf_llvm_ir_emit_inst(os, i, insns[i]);
    }
    bf_llvm_ir_emit_footer(os, insns.size());
    return os.str();
}
