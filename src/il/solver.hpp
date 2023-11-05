#pragma once
#include <vector>
#include <cstdint>
#include <unordered_map>

#include <triton/context.hpp>
#include <triton/llvmToTriton.hpp>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

namespace llvm
{
class Value;
};

namespace il
{
std::vector<uint64_t> get_possible_targets(llvm::Value* ret);
};
