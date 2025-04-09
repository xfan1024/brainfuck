#include "bf_runner_jit.h"
#include "bf_llvm_ir.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetSelect.h"

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

BfRunnerJit::BfRunnerJit()
{
    LlvmInitializeOnce();
}

void BfRunnerJit::compileCode()
{
    BfRunnerBfInsn::compileCode();

    mTimer.start("GEN llvm-ir");
    std::string llvmIr = bfLlvmIrGenerate(mInsns);
    mTimer.stop();

    if (mEnableIrEmit)
    {
        bfLlvmIrDumpToFile(mSourcePath, llvmIr);
    }

    mTimer.start("JIT compile");
    auto Buffer = MemoryBuffer::getMemBuffer(llvmIr, "in_memory_ir");
    SMDiagnostic Err;
    auto M = parseIR(Buffer->getMemBufferRef(), Err, mContext);
    if (!M)
    {
        errs() << "Error parsing IR: " << Err.getMessage() << "\n";
        exit(1);
    }

    auto JITOrErr = LLJITBuilder().create();
    if (!JITOrErr) {
        errs() << "Error creating JIT: " << toString(JITOrErr.takeError()) << "\n";
        exit(1);
    }
    mJIT = std::move(*JITOrErr);

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