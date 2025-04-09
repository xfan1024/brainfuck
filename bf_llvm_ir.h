#ifndef __bf_llvm_ir_h__
#define __bf_llvm_ir_h__

#include <string>
#include <vector>
#include "bf_runner_bf_insn.h"

void bfLlvmIrDumpToFile(const std::string &bf_file, const std::string &llvm_ir);
std::string bfLlvmIrGenerate(const std::vector<bf_insn> &insns);

#endif
