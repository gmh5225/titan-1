#include "flags_synthesis.hpp"
#include "il/solver.hpp"
#include "logger.hpp"
#include "utils.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>

#include <triton/llvmToTriton.hpp>

#include <optional>
#include <fstream>
#include <stack>

struct InstructionSlice
{
    static InstructionSlice get(llvm::Instruction* value)
    {
        InstructionSlice slice;

        std::stack<llvm::Instruction*> worklist{ { value } };
        std::set<llvm::Instruction*> known;

        while (!worklist.empty())
        {
            auto insn = worklist.top(); worklist.pop();
            // Skip if known.
            //
            if (known.contains(insn))
                continue;
            // Save.
            //
            known.insert(insn);
            slice.stream.push_back(insn);
            // Terminate if load/phi.
            //
            if (llvm::isa<llvm::LoadInst>(insn) || llvm::isa<llvm::PHINode>(insn))
            {
                slice.operands.push_back(insn);
                continue;
            }
            // Iterate use chain.
            //
            for (const auto& op : insn->operands())
            {
                if (auto op_insn = llvm::dyn_cast<llvm::Instruction>(op.get()))
                {
                    worklist.push(op_insn);
                }
            }
        }
        // Sort instructions by dominance.
        //
        llvm::DominatorTree dt(*value->getFunction());

        std::sort(slice.stream.begin(), slice.stream.end(), [&dt](const auto& a, const auto& b)
        {
            return dt.dominates(a, b);
        });
        return slice;
    }

    std::vector<llvm::Instruction*> stream;
    std::vector<llvm::Instruction*> operands;
};

#define GET_OR_CREATE_FUNCTION(name)                            \
    if (auto fn = module->getFunction(name))                    \
        return fn;                                              \
    auto fn = llvm::Function::Create(                           \
        llvm::FunctionType::get(i1, { ptr, ptr }, false),       \
        llvm::GlobalVariable::LinkageTypes::InternalLinkage,    \
        name,                                                   \
        module.get()                                            \
    );                                                          \
    auto bb  = llvm::BasicBlock::Create(*context, "body", fn);  \
    llvm::IRBuilder<> ir(bb);                                   \
    auto op0 = ir.CreateLoad(i64, fn->getOperand(0));           \
    auto op1 = ir.CreateLoad(i64, fn->getOperand(1));           \



llvm::Function* FlagsSynthesisPass::get_or_create_jo()
{
    GET_OR_CREATE_FUNCTION("jo");
    ir.CreateRet(
        ir.CreateExtractValue(
            ir.CreateCall(
                llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::sadd_with_overflow, i64),
                { op1, ir.CreateXor(op0, -1) }
            ),
            { 1 }
        )
    );
    return fn;
}

llvm::Function* FlagsSynthesisPass::get_or_create_js()
{
    GET_OR_CREATE_FUNCTION("js");
    ir.CreateRet(
        ir.CreateICmpSGT(
            ir.CreateSub(
                op0, op1
            ),
            llvm::ConstantInt::get(i64, -1)
        )
    );
    return fn;
}

llvm::Function* FlagsSynthesisPass::get_or_create_jns()
{
    GET_OR_CREATE_FUNCTION("jns");
    ir.CreateRet(
        ir.CreateICmpSLT(
            ir.CreateSub(
                op0, op1
            ),
            llvm::ConstantInt::get(i64, 0)
        )
    );
    return fn;
}

llvm::Function* FlagsSynthesisPass::get_or_create_je()
{
    GET_OR_CREATE_FUNCTION("je");
    ir.CreateRet(
        ir.CreateICmpEQ(
            op0, op1
        )
    );
    return fn;
}

llvm::Function* FlagsSynthesisPass::get_or_create_jne()
{
    GET_OR_CREATE_FUNCTION("jne");
    ir.CreateRet(
        ir.CreateICmpEQ(
            op1, op0
        )
    );
    return fn;
}

llvm::Function* FlagsSynthesisPass::get_or_create_jb()
{
    GET_OR_CREATE_FUNCTION("jb");
    ir.CreateRet(
        ir.CreateICmpUGT(
            op1, op0
        )
    );
    return fn;
}

llvm::Function* FlagsSynthesisPass::get_or_create_ja()
{
    GET_OR_CREATE_FUNCTION("ja");
    ir.CreateRet(
        ir.CreateICmpULT(op1, op0)
    );
    return fn;
}

llvm::Function* FlagsSynthesisPass::get_or_create_jl()
{
    GET_OR_CREATE_FUNCTION("jl");
    ir.CreateRet(
        ir.CreateICmpSGT(
            op1, op0
        )
    );
    return fn;
}

llvm::Function* FlagsSynthesisPass::get_or_create_jge()
{
    GET_OR_CREATE_FUNCTION("jge");
    ir.CreateRet(
        ir.CreateICmpSLT(
            op0, op1
        )
    );
    return fn;
}

llvm::Function* FlagsSynthesisPass::get_or_create_jle()
{
    GET_OR_CREATE_FUNCTION("jle");
    ir.CreateRet(
        ir.CreateICmpSLT(
            op1, op0
        )
    );
    return fn;
}

llvm::Function* FlagsSynthesisPass::get_or_create_jg()
{
    GET_OR_CREATE_FUNCTION("jg");
    ir.CreateRet(
        ir.CreateICmpSGT(
            op0, op1
        )
    );
    return fn;
}

// %1 = load i64, i64* %rax, align 8
// %2 = load i64, i64* %rbx, align 8
// %5 = xor i64 %1, -1
// %6 = add i64 %2, %5
// %7 = trunc i64 %6 to i32
// %8 = and i32 %7, 255
// %9 = call i32 @llvm.ctpop.i32(i32 %8) #15
// %10 = and i32 %9, 1
// %11 = icmp eq i32 %10, 0
//
llvm::Function* FlagsSynthesisPass::get_or_create_jp()
{
    GET_OR_CREATE_FUNCTION("jp");
    ir.CreateRet(
        ir.CreateICmpEQ(
            ir.CreateAnd(
                ir.CreateCall(
                    llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::ctpop, i32),
                    {
                        ir.CreateAnd(
                            ir.CreateTrunc(
                                ir.CreateAdd(
                                    op1,
                                    ir.CreateXor(
                                        op0, llvm::ConstantInt::get(i64, -1)
                                    )
                                ),
                                i32
                            ),
                            llvm::ConstantInt::get(i32, 255)
                        )
                    }
                ),
                llvm::ConstantInt::get(i32, 1)
            ),
            llvm::ConstantInt::get(i32, 0)
        )
    );
    return fn;
}

FlagsSynthesisPass::FlagsSynthesisPass()
    : context{ new llvm::LLVMContext }
    , module { new llvm::Module("flags", *context) }
    , i1 { llvm::IntegerType::getInt1Ty (*context) }
    , i32{ llvm::IntegerType::getInt32Ty(*context) }
    , i64{ llvm::IntegerType::getInt64Ty(*context) }
    , ptr{ llvm::PointerType::get(*context, 0)     }
{
}

llvm::PreservedAnalyses FlagsSynthesisPass::run(llvm::Function& fn, llvm::FunctionAnalysisManager& am)
{
    auto module = fn.getParent();
    auto layout = module->getDataLayout();

    for (auto& insn : llvm::instructions(fn))
    {
        if (auto br = llvm::dyn_cast<llvm::BranchInst>(&insn))
        {
            if (br->isConditional())
            {
                if (auto condition = llvm::dyn_cast<llvm::Instruction>(br->getOperand(0)))
                {
                    auto slice = InstructionSlice::get(condition);

                    // logger::info("slicing..");
                    // for (const auto& insn : slice.stream)
                    // {
                    //     insn->dump();
                    // }
                }
            }
        }
    }
    return llvm::PreservedAnalyses::none();
}
