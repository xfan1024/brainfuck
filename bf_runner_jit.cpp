#include "bf_runner_jit.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Support/TargetSelect.h"

#include "llvm/Transforms/Utils/PromoteMemToReg.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"

#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Host.h"

using namespace llvm;
using namespace llvm::orc;

static void LlvmInitializeOnce()
{
    // using atomic operations to ensure thread safety
    static std::atomic<bool> initialized(false);
    if (!initialized.exchange(true))
    {
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();
        InitializeNativeTargetAsmParser();
    }
}

#define BF_MEM_SIZE (1 << 20)
#define BF_ADDR_MASK (BF_MEM_SIZE - 1)

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

std::string bf_llvm_ir_generate(const std::vector<bf_insn> &insns)
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

BfRunnerJit::BfRunnerJit()
{
    LlvmInitializeOnce();
}

void BfRunnerJit::compileCode()
{
    BfRunnerBfInsn::compileCode();

    mTimer.start("GEN llvm-ir");
    std::string llvmIr = bf_llvm_ir_generate(mInsns);
    mTimer.stop();

    mTimer.start("JIT compile");
    auto Buffer = MemoryBuffer::getMemBuffer(llvmIr, "in_memory_ir");
    SMDiagnostic Err;
    auto M = parseIR(Buffer->getMemBufferRef(), Err, mContext);
    if (!M)
    {
        Err.print("LLVMJITExample", errs());
        exit(1);
    }

    legacy::PassManager PM;
    PM.add(createInstructionCombiningPass());
    PM.run(*M);

    auto JITOrErr = LLJITBuilder().create();
    if (!JITOrErr) {
        errs() << "Error creating JIT: " << toString(JITOrErr.takeError()) << "\n";
        exit(1);
    }
    mJIT = std::move(*JITOrErr);

    // 将优化后的 Module 添加到 JIT 中
    if (auto Err = mJIT->addIRModule(ThreadSafeModule(std::move(M), std::make_unique<LLVMContext>())))
    {
        errs() << "Error adding module: " << toString(std::move(Err)) << "\n";
        exit(1);
    }

    mTimer.stop();
}

void BfRunnerJit::run()
{
    typedef void (*writeByte_t)(BfRunner*, unsigned char);
    typedef unsigned char (*readByte_t)(BfRunner*);
    typedef void (*bfcode_t)(BfRunner*, uint8_t*, readByte_t, writeByte_t);
    std::vector<uint8_t> memory(BF_MEM_SIZE, 0);

    auto Sym = mJIT->lookup("bfcode");
    if (!Sym)
    {
        errs() << "Error looking up symbol 'bfcode': " << toString(Sym.takeError()) << "\n";
        exit(1);
    }

    bfcode_t bfcode = reinterpret_cast<bfcode_t>(Sym->getAddress());
    readByte_t readByte = BfRunner::readByte;
    writeByte_t writeByte = BfRunner::writeByte;

    mTimer.start("RUN native");
    bfcode(this, memory.data(), readByte, writeByte);
    mTimer.stop();
}