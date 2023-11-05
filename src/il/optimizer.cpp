#include "optimizer.hpp"
#include "logger.hpp"
#include "utils.hpp"

#include "passes/alias.hpp"
#include "passes/coalescing.hpp"
#include "passes/flags_synthesis.hpp"
#include "passes/deps.hpp"

#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Analysis/CycleAnalysis.h>
#include <llvm/Analysis/ScopedNoAliasAA.h>
#include <llvm/Analysis/BasicAliasAnalysis.h>
#include <llvm/Analysis/TypeBasedAliasAnalysis.h>
#include <llvm/Analysis/CFLAndersAliasAnalysis.h>
#include <llvm/Analysis/CFLSteensAliasAnalysis.h>
#include <llvm/Analysis/ScalarEvolutionAliasAnalysis.h>

#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Scalar/LoopRotation.h>

namespace il
{
void replace_undefined_variable(llvm::Function* fn)
{
    if (auto undef = fn->getParent()->getGlobalVariable("__undef"))
    {
        for (auto user : undef->users())
        {
            if (auto load = llvm::dyn_cast<llvm::LoadInst>(user))
            {
                if (auto parent = load->getParent()->getParent())
                {
                    if (parent == fn)
                    {
                        load->replaceAllUsesWith(llvm::UndefValue::get(load->getType()));
                    }
                }
            }
        }
    }
}

bool inline_intrinsics(llvm::Function* fn)
{
    std::set<llvm::CallInst*> calls_to_inline;
    for (auto& bb : *fn)
    {
        for (auto& ins : bb)
        {
            if (auto call = llvm::dyn_cast<llvm::CallInst>(&ins))
            {
                auto called_fn = call->getCalledFunction();
                if (called_fn->hasFnAttribute(llvm::Attribute::AlwaysInline) && !called_fn->isDeclaration())
                {
                    calls_to_inline.insert(call);
                }
            }
        }
    }
    for (auto call : calls_to_inline)
    {
        llvm::InlineFunctionInfo ifi;
        llvm::InlineFunction(*call, ifi);
    }
    return !calls_to_inline.empty();
}

void strip_names(llvm::Function* fn)
{
    for (auto& bb : *fn)
    {
        bb.setName("");
        for (auto& ins : bb)
        {
            if (ins.hasName()) ins.setName("");
        }
    }
}

template<typename M, typename A, typename O>
void exhaust_optimizations(M& manager,  A& analysis, O& object, uint64_t max_tries)
{
    auto inscount{ object.getInstructionCount() };
    auto tries{ 0u };
    while (true)
    {
        manager.run(object, analysis);
        if (inscount > object.getInstructionCount())
        {
            inscount = object.getInstructionCount();
            tries    = 0;
        }
        else if (tries++ > max_tries)
        {
            break;
        }
    }
}

void optimize_function(llvm::Function* fn, const opt_guide& guide)
{
    llvm::AAManager aam;
    llvm::PassBuilder pb;
    llvm::LoopPassManager lpm;
    llvm::LoopAnalysisManager lam;
    llvm::CGSCCAnalysisManager cam;
    llvm::ModuleAnalysisManager mam;
    llvm::FunctionAnalysisManager fam;

    auto ofpm = pb.buildFunctionSimplificationPipeline(guide.level, llvm::ThinOrFullLTOPhase::None);
    auto ompm = pb.buildModuleOptimizationPipeline(guide.level, llvm::ThinOrFullLTOPhase::None);

    ofpm.addPass(llvm::createFunctionToLoopPassAdaptor(llvm::LoopRotatePass(), true, true, true));
    // ofpm.addPass(MemoryCoalescingPass());
    ofpm.addPass(llvm::VerifierPass());

    while (inline_intrinsics(fn))
        ;

    if (guide.alias_analysis)
    {
        aam.registerFunctionAnalysis<SegmentsAA>();
        aam.registerFunctionAnalysis<llvm::BasicAA>();
        aam.registerFunctionAnalysis<llvm::ScopedNoAliasAA>();
        aam.registerFunctionAnalysis<llvm::TypeBasedAA>();
        aam.registerFunctionAnalysis<llvm::CFLAndersAA>();
        aam.registerFunctionAnalysis<llvm::CFLSteensAA>();
        fam.registerPass([]    { return SegmentsAA();   });
        fam.registerPass([aam] { return std::move(aam); });
    }

    pb.registerLoopAnalyses(lam);
    pb.registerCGSCCAnalyses(cam);
    pb.registerModuleAnalyses(mam);
    pb.registerFunctionAnalyses(fam);
    pb.crossRegisterProxies(lam, fam, cam, mam);

    exhaust_optimizations(ofpm, fam, *fn, 2);

    if (guide.remove_undef)
    {
        replace_undefined_variable(fn);
    }

    exhaust_optimizations(ofpm, fam, *fn, 5);

    if (guide.apply_dse)
    {
        ofpm.addPass(MemoryDependenciesPass());
        ofpm.addPass(FlagsSynthesisPass());
    }

    ofpm.run(*fn, fam);

    if (guide.strip_names)
        strip_names(fn);

    if (guide.run_on_module)
    {
        exhaust_optimizations(ompm, mam, *fn->getParent(), 5);
    }

    cam.clear();
    lam.clear();
    fam.clear();
    mam.clear();
}

void optimize_block_function(llvm::Function* fn)
{
    optimize_function(fn, {
        .strip_names = true,
        .level       = llvm::OptimizationLevel::O3
    });
}

void optimize_virtual_function(llvm::Function* fn)
{
    optimize_function(fn, {
        .remove_undef   = true,
        .run_on_module  = true,
        .strip_names    = true,
        .alias_analysis = true,
        .apply_dse      = true,
        .level          = llvm::OptimizationLevel::O3
    });
}
}
