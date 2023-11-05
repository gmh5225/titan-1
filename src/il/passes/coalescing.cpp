#include "coalescing.hpp"
#include "logger.hpp"

#include <map>
#include <vector>

#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Analysis/ScalarEvolutionExpressions.h>

llvm::Type* get_integer_type(int size, llvm::LLVMContext& context)
{
    switch (size)
    {
    case 1:
        return llvm::Type::getInt8Ty(context);
    case 2:
        return llvm::Type::getInt16Ty(context);
    case 4:
        return llvm::Type::getInt32Ty(context);
    case 8:
        return llvm::Type::getInt64Ty(context);
    default:
        logger::error("got unsupported size: {}", size);
    }
}

MemoryAccess::MemoryAccess(llvm::MemoryLocation location, const llvm::MemoryAccess* access, const llvm::SCEV* scev)
    : access_   { access }
    , scev_     { scev }
    , location_ { std::move(location) }
    , size_     { location_.Size.getValue() }
    , offset_   {}
    , supported_{}
{
    auto add = llvm::dyn_cast_or_null<llvm::SCEVAddExpr>(scev_);
    if (add && add->getNumOperands() == 3)
    {
        auto op0 = add->getOperand(0);
        auto op1 = add->getOperand(1);
        auto op2 = add->getOperand(2);
        // (-60 + %1 + @RAM).
        //
        if (op0->getSCEVType() == llvm::scConstant && op1->getSCEVType() == llvm::scUnknown && op2->getSCEVType() == llvm::scUnknown)
        {
            supported_ = true;
            offset_    = (int64_t)llvm::dyn_cast<llvm::SCEVConstant>(op0)->getValue()->getValue().getLimitedValue();
        }
    }
}

const llvm::MemoryAccess* MemoryAccess::access() const noexcept
{
    return access_;
}

const llvm::SCEV* MemoryAccess::scalar() const noexcept
{
    return scev_;
}

const llvm::MemoryLocation& MemoryAccess::location() const noexcept
{
    return location_;
}

uint64_t MemoryAccess::size() const noexcept
{
    return size_;
}

int64_t MemoryAccess::offset() const noexcept
{
    return offset_;
}

bool MemoryAccess::supported() const noexcept
{
    return supported_;
}

llvm::PreservedAnalyses MemoryCoalescingPass::run(llvm::Function& fn, llvm::FunctionAnalysisManager& am)
{
    auto& aam  = am.getResult<llvm::AAManager>(fn);
    auto& msaa = am.getResult<llvm::MemorySSAAnalysis>(fn).getMSSA();
    auto& se   = am.getResult<llvm::ScalarEvolutionAnalysis>(fn);
    auto& dt   = msaa.getDomTree();
    auto pdt   = llvm::PostDominatorTree(fn);

    bool modified = false;

    std::vector<llvm::StoreInst*> garbage;

    for (auto& block : fn)
    {
        // Skip if the block does not have memory accesses.
        //
        if (msaa.getBlockAccesses(&block) == nullptr)
            continue;
        // Find two sequential stores with the same size:
        // ; 2 = MemoryDef(1)
        //   store i16 0, ptr %12, align 1, !noalias !38
        //   %13 = trunc i64 %2 to i16
        // ; 3 = MemoryDef(2)
        //   store i16 %13, ptr %10, align 1, !noalias !38
        //
        // And replace them with 1 store double the size.
        //
        std::vector<const llvm::MemoryAccess*> accs;
        for (const auto& acc : *msaa.getBlockAccesses(&block))
        {
            accs.push_back(&acc);
        }

        for (int i = 0; i < accs.size() - 1; i++)
        {
            auto def0 = llvm::dyn_cast_or_null<llvm::MemoryDef>(accs[i]);
            auto def1 = llvm::dyn_cast_or_null<llvm::MemoryDef>(accs[i + 1]);

            if (def0 && def1)
            {
                auto insn0 = llvm::dyn_cast_or_null<llvm::StoreInst>(def0->getMemoryInst());
                auto insn1 = llvm::dyn_cast_or_null<llvm::StoreInst>(def1->getMemoryInst());

                auto store0 = MemoryAccess(llvm::MemoryLocation::get(insn0), msaa.getMemoryAccess(insn0), se.getSCEV(insn0->getPointerOperand()));
                auto store1 = MemoryAccess(llvm::MemoryLocation::get(insn1), msaa.getMemoryAccess(insn1), se.getSCEV(insn1->getPointerOperand()));

                if (store0.supported() && store1.supported() && store0.size() == store1.size() && store0.size() < 8)
                {
                    if (store1.offset() + store1.size() == store0.offset())
                    {
                        logger::debug("Found two sequential stores {} {}:", store0.offset(), store1.offset());
                        insn0->dump();
                        insn1->dump();
                        llvm::IRBuilder<> ir(insn1->getNextNode());
                        auto op0_zext  = ir.CreateZExt(insn0->getValueOperand(), ir.getIntNTy(store0.size() * 8 * 2));
                        auto op1_zext  = ir.CreateZExt(insn1->getValueOperand(), ir.getIntNTy(store1.size() * 8 * 2));
                        auto op0_shl   = ir.CreateShl(op0_zext, store0.size() * 8, "", true, true);
                        auto fin_or    = ir.CreateOr(op0_shl, op1_zext);
                        auto fin_store = ir.CreateStore(fin_or, insn1->getPointerOperand());
                        garbage.push_back(insn0);
                        garbage.push_back(insn1);

                        i++;
                        modified = true;
                    }
                }
            }
        }
    }
    for (auto& store : garbage)
    {
        store->eraseFromParent();
    }
    return modified ? llvm::PreservedAnalyses::none() : llvm::PreservedAnalyses::all();
}
