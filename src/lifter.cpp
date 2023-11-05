#include "lifter.hpp"
#include "logger.hpp"
#include "utils.hpp"

#include "vm/instruction.hpp"
#include "vm/routine.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <map>

llvm::cl::opt<std::string> intrinsics("i",
    llvm::cl::desc("Path to vmprotect intrinsics file"),
    llvm::cl::value_desc("intrinsics"),
    llvm::cl::Required);

llvm::Value* ReturnArguments::return_address() const noexcept
{
    return ret;
}

llvm::Value* ReturnArguments::program_counter() const noexcept
{
    return rip;
}

Lifter::Lifter() : ir(context)
{
    llvm::SMDiagnostic err;
    auto parsed = llvm::parseIRFile(intrinsics, err, context);
    if (parsed == nullptr)
    {
        logger::error("Lifter::Lifter: Failed to parse intrinsics file");
    }
    module = std::move(parsed);
    // Extract helper functions.
    //
    helper_lifted_fn      = module->getFunction("VirtualFunction");
    helper_empty_block_fn = module->getFunction("VirtualStubEmpty");
    helper_block_fn       = module->getFunction("VirtualStub");
    helper_keep_fn        = module->getFunction("KeepReturn");
    helper_slice_fn       = module->getFunction("SlicePC");
    helper_undef          = module->getGlobalVariable("__undef");
    if (helper_lifted_fn == nullptr)
        logger::error("Failed to find VirtualFunction function");
    if (helper_empty_block_fn == nullptr)
        logger::error("Failed to find VirtualStubEmpty function");
    if (helper_block_fn == nullptr)
        logger::error("Failed to find VirtualStub function");
    if (helper_keep_fn == nullptr)
        logger::error("Failed to find KeepPC function");
    if (helper_slice_fn == nullptr)
        logger::error("Failed to find SlicePC function");
    if (helper_undef == nullptr)
        logger::error("Failed to find global undef variable");
    // Collect semantics functions.
    //
    for (const auto& glob : module->globals())
    {
        const auto& name = glob.getName();
        if (name.startswith("SEM_") && glob.isConstant() && glob.getType()->isPointerTy())
        {
            const auto initializer = glob.getInitializer();
            auto resolved_fn       = module->getFunction(initializer->getName());
            if (resolved_fn == nullptr)
            {
                logger::error("Lifter::Lifter: Failed to resolve function for global {}", name.str());
            }
            sems.emplace(name.str().substr(4), resolved_fn);
        }
    }
}

llvm::Function* Lifter::lift_basic_block(vm::BasicBlock* vblock)
{
    // Copy empty block function.
    //
    function = clone(helper_empty_block_fn);
    // Remove bad attributes. Don't know if it does anything but still.
    //
    for (auto& arg : function->args())
    {
        arg.removeAttr(llvm::Attribute::ReadNone);
        arg.removeAttr(llvm::Attribute::ReadOnly);
    }
    function->removeFnAttr(llvm::Attribute::ReadNone);
    // Remove entry basic block and insert new one.
    //
    function->getEntryBlock().eraseFromParent();

    ir.SetInsertPoint(llvm::BasicBlock::Create(context, "lifted_bb", function));
    // Lift instruction stream.
    //
    for (const auto& insn : *vblock)
    {
        std::visit(*this, insn);
    }
    // Return VIP.
    //
    ir.CreateRet(ir.CreateLoad(function->getReturnType(), vip()));
    return function;
}

llvm::Function* Lifter::build_function(const vm::Routine* rtn, uint64_t target_block)
{
    function = clone(helper_empty_block_fn);
    function->getEntryBlock().eraseFromParent();
    auto block = llvm::BasicBlock::Create(context, "entry", function);

    std::vector<llvm::Value*> args;
    for (auto& arg : function->args())
        args.push_back(&arg);
    // Create empty basic blocks for each basic block in the routine.
    //
    std::map<uint64_t, llvm::BasicBlock*> blocks;
    for (const auto [vip, bb] : *rtn)
    {
        blocks.emplace(vip, llvm::BasicBlock::Create(context, fmt::format("bb_0x{:x}", vip), function));
    }
    // Link together llvm basic blocks based on edges in routine and populate with calls to lifted functions.
    //
    for (auto [vip, bb] : blocks)
    {
        ir.SetInsertPoint(bb);
        auto vblock = rtn->blocks.at(vip);
        if (vblock->lifted != nullptr)
        {
            auto pc = ir.CreateCall(vblock->lifted, args);
            // Check if we are building partial function and if so, make a call to KeepPC.
            //
            if (vblock->vip() == target_block && target_block != vm::invalid_vip)
            {
                // Load rsp value.
                //
                auto ret = create_memory_read_64(ir.CreateLoad(ir.getInt64Ty(), vsp()));

                pc = ir.CreateCall(helper_keep_fn, { pc, ret });
            }
            // Link successors with the current block.
            //
            switch (vblock->next.size())
            {
                case 0:
                {
                    ir.CreateRet(pc);
                    break;
                }
                case 1:
                {
                    auto dst_vip = vblock->next.at(0)->vip();
                    auto dst_blk = blocks.at(dst_vip);
                    if (vblock->vip() == target_block && target_block != vm::invalid_vip)
                    {
                        // Create dummy basic block.
                        //
                        auto dummy_bb = llvm::BasicBlock::Create(context, fmt::format("bb_dummy_0x{:x}", vblock->vip()), function);
                        llvm::ReturnInst::Create(context, pc, dummy_bb);
                        auto cmp = ir.CreateICmpEQ(pc, ir.getInt64(dst_vip));
                        ir.CreateCondBr(cmp, dst_blk, dummy_bb);
                    }
                    else
                    {
                        ir.CreateBr(dst_blk);
                    }
                    break;
                }
                case 2:
                {
                    auto dst_blk_1 = blocks.at(vblock->next.at(0)->vip());
                    auto dst_blk_2 = blocks.at(vblock->next.at(1)->vip());
                    auto cmp       = ir.CreateICmpEQ(pc, ir.getInt64(vblock->next.at(0)->vip()));
                    ir.CreateCondBr(cmp, dst_blk_1, dst_blk_2);
                    break;
                }
                default:
                    llvm_unreachable("Switch statement is currently not supported.");
            }
        }
        else
        {
            ir.CreateRet(ir.getInt64(0xdeadbeef));
        }
    }
    llvm::BranchInst::Create(blocks.at(rtn->entry->vip()), block);
    // If its a partial function, we want to slice PC, otherwise build "final function".
    //
    if (target_block != vm::invalid_vip)
        return make_slice(function);
    return make_final(function, rtn->entry->vip());
}

ReturnArguments Lifter::get_return_args(llvm::Function* fn) const
{
    for (auto& block : *fn)
    {
        for (auto& ins : block)
        {
            if (auto call = llvm::dyn_cast<llvm::CallInst>(&ins))
            {
                if (call->getCalledFunction() == helper_keep_fn)
                {
                    return ReturnArguments(call->getOperand(0), call->getOperand(1));
                }
            }
        }
    }
    logger::error("Failed to find call to KeepReturnAddress funtion in {}", fn->getName().str());
}

llvm::Argument* Lifter::arg(llvm::Function* fn, const std::string& name)
{
    for (auto& arg : fn->args())
        if (arg.getName().equals(name))
            return &arg;
    return nullptr;
}

llvm::Argument* Lifter::arg(const std::string& name)
{
    return arg(function, name);
}

llvm::Function* Lifter::clone(llvm::Function* fn)
{
    llvm::ValueToValueMapTy map;
    return llvm::CloneFunction(fn, map);
}

llvm::Function* Lifter::sem(const std::string& name) const
{
    if (sems.find(name) == sems.end())
        logger::error("Failed to find {} semantic", name);
    return sems.at(name);
}

llvm::Argument* Lifter::vip()
{
    return arg("vip");
}

llvm::Argument* Lifter::vsp()
{
    return arg("vsp");
}

llvm::Argument* Lifter::vregs()
{
    return arg("vmregs");
}

llvm::Function* Lifter::make_slice(llvm::Function* fn)
{
    auto slice = clone(helper_slice_fn);
    for (auto& ins : slice->getEntryBlock())
    {
        if (auto call = llvm::dyn_cast<llvm::CallInst>(&ins))
        {
            if (call->getCalledFunction() == helper_block_fn)
            {
                call->setCalledFunction(fn);
                break;
            }
        }
    }
    return slice;
}

llvm::Function* Lifter::make_final(llvm::Function* fn, uint64_t addr)
{
    auto* final = clone(helper_lifted_fn);
    for (auto& ins : final->getEntryBlock())
    {
        if (auto* call = llvm::dyn_cast<llvm::CallInst>(&ins))
        {
            if (call->getCalledFunction() == helper_block_fn)
            {
                call->setCalledFunction(fn);
                break;
            }
        }
    }
    return final;
}

void Lifter::operator()(const vm::Add& insn)
{
    ir.CreateCall(sem(fmt::format("ADD_{}", insn.size())), { vsp() });
}

void Lifter::operator()(const vm::Shl& insn)
{
    ir.CreateCall(sem(fmt::format("SHL_{}", insn.size())), { vsp() });
}

void Lifter::operator()(const vm::Shr& insn)
{
    ir.CreateCall(sem(fmt::format("SHR_{}", insn.size())), { vsp() });
}

void Lifter::operator()(const vm::Ldr& insn)
{
    ir.CreateCall(sem(fmt::format("LOAD_{}", insn.size())), { vsp() });
}

void Lifter::operator()(const vm::Str& insn)
{
    ir.CreateCall(sem(fmt::format("STORE_{}", insn.size())), { vsp() });
}

void Lifter::operator()(const vm::Nor& insn)
{
    ir.CreateCall(sem(fmt::format("NOR_{}", insn.size())), { vsp() });
}

void Lifter::operator()(const vm::Nand& insn)
{
    ir.CreateCall(sem(fmt::format("NAND_{}", insn.size())), { vsp() });
}

void Lifter::operator()(const vm::Shrd& insn)
{
    ir.CreateCall(sem(fmt::format("SHRD_{}", insn.size())), { vsp() });
}

void Lifter::operator()(const vm::Shld& insn)
{
    ir.CreateCall(sem(fmt::format("SHLD_{}", insn.size())), { vsp() });
}

void Lifter::operator()(const vm::Push& insn)
{
    auto size = insn.size();

    if (insn.op().is_immediate())
    {
        ir.CreateCall(sem(fmt::format("PUSH_IMM_{}", size)), { vsp(), ir.getInt(llvm::APInt(size, insn.op().imm().value())) });
    }
    else if (insn.op().is_physical())
    {
        auto reg = arg(insn.op().phy().name());
        auto ldr = ir.CreateLoad(ir.getInt64Ty(), reg);
        ir.CreateCall(sem(fmt::format("PUSH_REG_{}", size)), { vsp(), ldr });
    }
    else if (insn.op().is_virtual())
    {
        auto num = insn.op().vrt().number();
        auto off = insn.op().vrt().offset();
        auto gep = ir.CreateInBoundsGEP(vregs()->getType(), vregs(), { ir.getInt(llvm::APInt(size, num)) });
        auto ldr = ir.CreateLoad(ir.getInt64Ty(), gep);
        ir.CreateCall(sem(fmt::format("PUSH_VREG_{}_{}", size, off)), { vsp(), ldr });
    }
    else if (insn.op().is_vsp())
    {
        ir.CreateCall(sem(fmt::format("PUSH_VSP_{}", size)), { vsp() });
    }
    else
    {
        logger::error("Lifter::operator(): Unsupported Push operand.");
    }
}

void Lifter::operator()(const vm::Pop& insn)
{
    auto size = insn.size();

    if (insn.op().is_physical())
    {
        ir.CreateCall(sem(fmt::format("POP_REG_{}", size)), { vsp(), arg(insn.op().phy().name()) });
    }
    else if (insn.op().is_virtual())
    {
        auto num = insn.op().vrt().number();
        auto off = insn.op().vrt().offset();
        auto gep = ir.CreateInBoundsGEP(vregs()->getType(), vregs(), { ir.getInt(llvm::APInt(size, num)) });
        ir.CreateCall(sem(fmt::format("POP_VREG_{}_{}", size, off)), { vsp(), gep });
    }
    else if (insn.op().is_vsp())
    {
        ir.CreateCall(sem(fmt::format("POP_VSP_{}", size)), { vsp() });
    }
    else
    {
        logger::error("Lifter::operator(): Unsupported Pop operand.");
    }
}

void Lifter::operator()(const vm::Jmp& insn)
{
    ir.CreateCall(sem("JMP"), { vsp(), vip() });
}

void Lifter::operator()(const vm::Ret& insn)
{
    ir.CreateCall(sem("RET"), { vsp(), vip() });
}

void Lifter::operator()(const vm::Jcc& insn)
{
    if (insn.direction() == vm::jcc_e::up)
    {
        ir.CreateCall(sem("JCC_INC"), { vsp(), vip() });
    }
    else
    {
        ir.CreateCall(sem("JCC_DEC"), { vsp(), vip() });
    }
}

void Lifter::operator()(const vm::Exit& insn)
{
    for (const auto& reg : insn.regs())
    {
        (*this)(reg);
    }
}

void Lifter::operator()(const vm::Enter& insn)
{
    for (const auto& reg : insn.regs())
    {
        (*this)(reg);
    }
}

llvm::Value* Lifter::create_memory_read_64(llvm::Value* address)
{
    auto ram = module->getGlobalVariable("RAM");
    auto gep = ir.CreateInBoundsGEP(ram->getValueType(), ram, { ir.getInt64(0), address });
    return ir.CreateLoad(ir.getInt64Ty(), gep);
}

llvm::Value* Lifter::create_memory_write_64(llvm::Value* address, llvm::Value* ptr)
{
    auto ram = module->getGlobalVariable("RAM");
    auto gep = ir.CreateInBoundsGEP(ram->getValueType(), ram, { ir.getInt64(0), address });
    return ir.CreateStore(ir.CreateLoad(ir.getInt64Ty(), ptr), gep);
}

std::vector<llvm::BasicBlock*> Lifter::get_exit_blocks(llvm::Function* fn) const
{
    std::vector<llvm::BasicBlock*> exits;

    for (auto& bb : *fn)
    {
        if (auto ret = llvm::dyn_cast<llvm::ReturnInst>(bb.getTerminator()))
        {
            exits.push_back(&bb);
        }
    }
    return exits;
}

// NOTE: This is just for testing.
//
void Lifter::create_external_call(llvm::Function* fn, const std::string& name)
{
    function   = fn;
    auto exits = get_exit_blocks(fn);
    if (exits.size() != 1)
        logger::error("Invalid number ({}) of exit blocks in a function {}", exits.size(), fn->getName().str());
    auto term = exits.back();
    // Insert call right before the ret instruction.
    //
    ir.SetInsertPoint(term->getTerminator()->getPrevNode());

    auto callee_ty = llvm::FunctionType::get(ir.getInt64Ty(), { ir.getInt64Ty(), /* ir.getInt64Ty(), ir.getInt64Ty(), ir.getInt64Ty() */ }, false);
    auto callee_fn = llvm::Function::Create(callee_ty, llvm::GlobalVariable::LinkageTypes::ExternalLinkage, name, module.get());
    // Mark this function as `ReadNone` meaning it does not read memory. This attribute allows for some optimizations to be applied.
    // ref: http://formalverification.cs.utah.edu/llvm_doxy/2.9/namespacellvm_1_1Attribute.html
    //
    callee_fn->addFnAttr(llvm::Attribute::ReadNone);
    // Pop function call address from the stack and mark the slot with __undef.
    // This will remove useless store in the final function.
    //
    ir.CreateCall(sem("STACK_POP_64"), { vsp() });

    auto call = ir.CreateCall(callee_fn, {
        ir.CreateLoad(ir.getInt64Ty(), arg("rcx")),
        // ir.CreateLoad(ir.getInt64Ty(), arg("rdx")),
        // ir.CreateLoad(ir.getInt64Ty(), arg("r8")),
        // ir.CreateLoad(ir.getInt64Ty(), arg("r9"))
    });
    ir.CreateStore(call, arg("rax"));
}
