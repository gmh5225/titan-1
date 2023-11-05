#include "alias.hpp"
#include "logger.hpp"

#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/PatternMatch.h>

using namespace llvm::PatternMatch;

#include <set>
#include <stack>

enum class pointer_t
{
    unknown,
    memory_array,
    memory_slot,
    stack_array,
    stack_slot
};

bool is_stack_slot(const llvm::Value* ptr)
{
    llvm::Value* value          = nullptr;
    llvm::ConstantInt* constant = nullptr;

    auto pattern0 = m_Add(m_Load(m_Value(value)), m_ConstantInt(constant));
    auto pattern1 = m_Add(m_Value(value), m_ConstantInt(constant));
    auto pattern2 = m_Load(m_Value(value));

    if (pattern0.match(ptr) ||
        pattern1.match(ptr) ||
        pattern2.match(ptr))
    {
        if (auto arg = llvm::dyn_cast<llvm::Argument>(value))
        {
            if (arg->getName().endswith("sp"))
            {
                return true;
            }
        }
    }
    return false;
}

pointer_t get_pointer_type(const llvm::Value* ptr)
{
    if (auto gep = llvm::dyn_cast<llvm::GEPOperator>(ptr))
    {
        if (auto gv = llvm::dyn_cast<llvm::GlobalVariable>(gep->getPointerOperand()))
        {
            if (gv->getName() == "RAM" && gep->getNumIndices() == 2)
            {
                if (is_stack_slot(gep->getOperand(2)))
                    return pointer_t::stack_slot;

                std::set<const llvm::Value*>    known;
                std::stack<const llvm::Value*>  worklist{ { gep->getOperand(2) } };
                std::vector<const llvm::Value*> bases;

                while (!worklist.empty())
                {
                    auto value = worklist.top(); worklist.pop();

                    if (known.contains(value))
                        continue;
                    known.insert(value);

                    if (auto load = llvm::dyn_cast<llvm::LoadInst>(value))
                    {
                        bases.push_back(load->getPointerOperand());
                        continue;
                    }
                    else if (auto arg = llvm::dyn_cast<llvm::Argument>(value))
                    {
                        bases.push_back(arg);
                        continue;
                    }
                    else if (auto call = llvm::dyn_cast<llvm::CallInst>(value))
                    {
                        auto name = call->getCalledFunction()->getName();
                        if (!name.startswith("llvm.ctpop") && !name.startswith("llvm.fshr") && !name.startswith("llvm.fshl"))
                        {
                            logger::warn("unknown pointer call instruction:");
                            value->dump();
                            return pointer_t::unknown;
                        }
                    }
                    else if (!llvm::isa<llvm::BinaryOperator>(value)
                        && !llvm::isa<llvm::SelectInst>(value)
                        && !llvm::isa<llvm::TruncInst>(value)
                        && !llvm::isa<llvm::ZExtInst>(value)
                        && !llvm::isa<llvm::SExtInst>(value)
                        && !llvm::isa<llvm::ICmpInst>(value)
                        && !llvm::isa<llvm::PHINode>(value))
                    {
                        logger::warn("unknown instruction:");
                        value->dump();
                        return pointer_t::unknown;
                    }

                    if (auto insn = llvm::dyn_cast<llvm::Instruction>(value))
                    {
                        for (const auto& use : insn->operands())
                        {
                            if (llvm::isa<llvm::Instruction>(use.get()) || llvm::isa<llvm::Argument>(use.get()))
                                worklist.push(use.get());
                        }
                    }
                }

                if (bases.size() == 2)
                    return pointer_t::memory_array;
            }
        }
    }
    return pointer_t::unknown;
}



bool SegmentsAAResult::invalidate(llvm::Function& f, const llvm::PreservedAnalyses& pa, llvm::FunctionAnalysisManager::Invalidator& inv)
{
    return false;
}

/*
Differentiate between:
- memory array
- memory slot
- stack array
- stack slot
*/
llvm::AliasResult SegmentsAAResult::alias(const llvm::MemoryLocation& loc_a, const llvm::MemoryLocation& loc_b, llvm::AAQueryInfo& info)
{
    auto a_ty = get_pointer_type(loc_a.Ptr);
    auto b_ty = get_pointer_type(loc_b.Ptr);

    if (a_ty != pointer_t::unknown && b_ty != pointer_t::unknown && a_ty != b_ty)
    {
        return llvm::AliasResult::NoAlias;
    }
    return AAResultBase::alias(loc_a, loc_b, info);
}

llvm::AnalysisKey SegmentsAA::Key;

SegmentsAAResult SegmentsAA::run(llvm::Function& f, llvm::FunctionAnalysisManager& fam)
{
    return SegmentsAAResult();
}
