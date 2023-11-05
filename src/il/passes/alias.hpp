#pragma once

#include <llvm/IR/PassManager.h>
#include <llvm/Analysis/AliasAnalysis.h>

// Based on https://secret.club/2021/09/08/vmprotect-llvm-lifting-3.html#segmentsaa
//
struct SegmentsAAResult : public llvm::AAResultBase<SegmentsAAResult>
{
    bool invalidate(llvm::Function& f, const llvm::PreservedAnalyses& pa, llvm::FunctionAnalysisManager::Invalidator& inv);
    llvm::AliasResult alias(const llvm::MemoryLocation& loc_a, const llvm::MemoryLocation& loc_b, llvm::AAQueryInfo& info);

private:
    friend llvm::AAResultBase<SegmentsAAResult>;
};

struct SegmentsAA final : public llvm::AnalysisInfoMixin<SegmentsAA>
{
    using Result = SegmentsAAResult;

    SegmentsAAResult run(llvm::Function& f, llvm::FunctionAnalysisManager& fam);

    static llvm::AnalysisKey Key;

private:
    friend llvm::AnalysisInfoMixin<SegmentsAA>;
};
