#pragma once

#include <llvm/IR/PassManager.h>

// Based on https://godbolt.org/z/aYdc3xhn9.
//
struct FlagsSynthesisPass final : public llvm::PassInfoMixin<FlagsSynthesisPass>
{
    FlagsSynthesisPass();
    llvm::PreservedAnalyses run(llvm::Function& fn, llvm::FunctionAnalysisManager& fam);

private:
    llvm::Function* get_or_create_jo();
    /* llvm::Function* get_or_create_jno(); */
    llvm::Function* get_or_create_js();
    llvm::Function* get_or_create_jns();
    llvm::Function* get_or_create_je();
    llvm::Function* get_or_create_jne();
    llvm::Function* get_or_create_jb();
    /* llvm::Function* get_or_create_jnb(); */
    /* llvm::Function* get_or_create_jna(); */
    llvm::Function* get_or_create_ja();
    llvm::Function* get_or_create_jl();
    llvm::Function* get_or_create_jge();
    llvm::Function* get_or_create_jle();
    llvm::Function* get_or_create_jg();
    llvm::Function* get_or_create_jp();
    /* llvm::Function* get_or_create_jnp(); */

    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;

    llvm::Type* i1;
    llvm::Type* i32;
    llvm::Type* i64;
    llvm::Type* ptr;
};
