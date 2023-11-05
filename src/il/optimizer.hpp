#pragma once

namespace llvm
{
class Function;
class Module;
};

#include <llvm/Passes/OptimizationLevel.h>

namespace il
{
struct opt_guide
{
    bool remove_undef;
    bool run_on_module;
    bool strip_names;
    bool alias_analysis;
    bool apply_dse;
    llvm::OptimizationLevel level;
};

void optimize_function(llvm::Function* fn, const opt_guide& guide);
void optimize_block_function(llvm::Function* fn);
void optimize_virtual_function(llvm::Function* fn);
};
