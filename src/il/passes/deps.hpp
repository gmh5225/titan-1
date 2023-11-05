#pragma once

#include <llvm/IR/PassManager.h>

struct MemoryDependenciesPass final : public llvm::PassInfoMixin<MemoryDependenciesPass>
{
    llvm::PreservedAnalyses run(llvm::Function& fn, llvm::FunctionAnalysisManager& fam);
};
