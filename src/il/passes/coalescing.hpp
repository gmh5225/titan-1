#pragma once

#include <llvm/IR/PassManager.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Analysis/MemorySSA.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/ScalarEvolutionAliasAnalysis.h>

struct MemoryAccess
{
    MemoryAccess(llvm::MemoryLocation location, const llvm::MemoryAccess* access, const llvm::SCEV* scev);

    const llvm::MemoryAccess*   access()      const noexcept;
    const llvm::SCEV*           scalar()      const noexcept;
    const llvm::MemoryLocation& location()    const noexcept;
    uint64_t                    size()        const noexcept;
    int64_t                     offset()      const noexcept;
    bool                        supported()   const noexcept;

private:
    const llvm::MemoryAccess*  access_;
    const llvm::SCEV*          scev_;
    const llvm::MemoryLocation location_;
    // Size of the memory load.
    //
    uint64_t size_;
    // Offset within RAM. e.g. in case of SCEV (-60 + %1 + @RAM) offset will be -60.
    //
    int64_t offset_;
    // If load is supported by MemoryCoalescingPass.
    //
    bool supported_;
};

struct MemoryCoalescingPass final : public llvm::PassInfoMixin<MemoryCoalescingPass>
{
    llvm::PreservedAnalyses run(llvm::Function &fn, llvm::FunctionAnalysisManager &am);
};
