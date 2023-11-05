#include "deps.hpp"
#include "logger.hpp"
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/MemoryDependenceAnalysis.h>
#include <llvm/Analysis/ScalarEvolutionAliasAnalysis.h>

llvm::PreservedAnalyses MemoryDependenciesPass::run(llvm::Function& fn, llvm::FunctionAnalysisManager& am)
{
    auto& mda = am.getResult<llvm::MemoryDependenceAnalysis>(fn);
    for (auto& bb : fn)
    {
        for (auto& insn : bb)
        {
            if (auto store = llvm::dyn_cast<llvm::StoreInst>(&insn))
            {
                auto dep  = mda.getDependency(store);
                auto insn = dep.getInst();
                if (insn)
                {
                    logger::debug("memory dependence:");
                    store->dump();
                    insn->dump();
                }
                else
                {
                    logger::debug("no memory dependence:");
                    store->dump();
                }
            }
        }
    }
    return llvm::PreservedAnalyses::all();
}
