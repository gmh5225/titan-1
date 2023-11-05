#include "explorer.hpp"
#include "il/optimizer.hpp"
#include "il/solver.hpp"
#include "asserts.hpp"
#include "utils.hpp"

static constexpr auto stack_base = 0x10000;

Explorer::Explorer(std::shared_ptr<Lifter> lifter, std::shared_ptr<Tracer> tracer)
    : lifter(lifter), tracer(tracer), block(nullptr), terminate(false)
{
}

std::unique_ptr<vm::Routine> Explorer::explore(uint64_t address)
{
    tracer->write(tracer->rip_register(), address);
    tracer->write(tracer->rsp_register(), stack_base);

    block = vm::Routine::begin(address);

    std::visit(*this, tracer->step(step_t::stop_before_branch));

    worklist.push(address);
    snapshots.emplace(address, std::move(tracer));

    while (!worklist.empty())
    {
        address = worklist.top(); worklist.pop();

        if (explored.count(address))
        {
            logger::warn("block 0x{:x} already explored.", address);
            continue;
        }
        explored.insert(address);

        block  = block->owner->blocks.at(address);
        tracer = snapshots.at(address);

        if (block->lifted != nullptr)
        {
            reprove_block();
            continue;
        }

        logger::debug("exploring 0x{:x}", address);

        while (!terminate)
        {
            // logger::info("execute: 0x{:x}", tracer->rip());
            // Process instruction.
            //
            std::visit(*this, tracer->step(step_t::stop_before_branch));
        }
        terminate = false;

        for (const auto& reprove : get_reprove_blocks())
        {
            logger::info("\treprove -> 0x{:x}", reprove);
            worklist.push(reprove);
            explored.erase(reprove);
        }
    }
    return std::unique_ptr<vm::Routine>{ block->owner };
}

void Explorer::operator()(vm::Add&& insn)
{
    logger::info("{:<5} {:<2}", "add", insn.size());
    block->add(std::move(insn));
}

void Explorer::operator()(vm::Shl&& insn)
{
    logger::info("{:<5} {:<2}", "shl", insn.size());
    block->add(std::move(insn));
}

void Explorer::operator()(vm::Shr&& insn)
{
    logger::info("{:<5} {:<2}", "shr", insn.size());
    block->add(std::move(insn));
}

void Explorer::operator()(vm::Ldr&& insn)
{
    logger::info("{:<5} {:<2}", "ldr", insn.size());
    block->add(std::move(insn));
}

void Explorer::operator()(vm::Str&& insn)
{
    logger::info("{:<5} {:<2}", "str", insn.size());
    block->add(std::move(insn));
}

void Explorer::operator()(vm::Nor&& insn)
{
    logger::info("{:<5} {:<2}", "nor", insn.size());
    block->add(std::move(insn));
}

void Explorer::operator()(vm::Nand&& insn)
{
    logger::info("{:<5} {:<2}", "nand", insn.size());
    block->add(std::move(insn));
}

void Explorer::operator()(vm::Shrd&& insn)
{
    logger::info("{:<5} {:<2}", "shrd", insn.size());
    block->add(std::move(insn));
}

void Explorer::operator()(vm::Shld&& insn)
{
    logger::info("{:<5} {:<2}", "shld", insn.size());
    block->add(std::move(insn));
}

void Explorer::operator()(vm::Push&& insn)
{
    logger::info("{:<5} {:<2} {}", "push", insn.size(), insn.op().to_string());
    block->add(std::move(insn));
}

void Explorer::operator()(vm::Pop&& insn)
{
    logger::info("{:<5} {:<2} {}", "pop", insn.size(), insn.op().to_string());
    block->add(std::move(insn));
}

void Explorer::operator()(vm::Jmp&& insn)
{
    logger::info("jmp");
    block->lifted = lifter->lift_basic_block(block);
    il::optimize_block_function(block->lifted);
    // Execute branch instruction.
    //
    tracer->step(step_t::execute_branch);
    // Fork block and continue executing from new one.
    //
    auto vip = tracer->vip();
    block->fork(vip);
    worklist.push(vip);
    snapshots.emplace(vip, tracer->fork());
    // Terminate current block.
    //
    terminate = true;
}

void Explorer::operator()(vm::Ret&& insn)
{
    block->add(std::move(insn));
    // Terminate current block.
    //
    terminate = true;
}

void Explorer::operator()(vm::Jcc&& insn)
{
    logger::info("jcc {}", insn.direction() == vm::jcc_e::up ? "up" : "down");
    block->add(insn);
    // Lift basic block.
    //
    block->lifted = lifter->lift_basic_block(block);
    il::optimize_block_function(block->lifted);
    // Extract targets.
    //
    auto slice = lifter->build_function(block->owner, block->vip());
    il::optimize_block_function(slice);

    auto ret = lifter->get_return_args(slice);

    for (const auto target : il::get_possible_targets(ret.program_counter()))
    {
        logger::info("\tjcc -> 0x{:x}", target);

        auto fork = tracer->fork();
        fork->write(fork->vsp(), target - (insn.direction() == vm::jcc_e::up ? 1 : -1) * 4);
        // Execute branch instruction.
        //
        fork->step(step_t::execute_branch);

        block->fork(target);
        worklist.push(target);
        snapshots.insert({ target, std::move(fork) });
    }
    // Terminate current block.
    //
    terminate = true;

    slice->eraseFromParent();
}

void Explorer::operator()(vm::Exit&& insn)
{
    for (const auto& reg : insn.regs())
    {
        logger::info("{:<5} {:<2} {}", "pop", reg.size(), reg.op().to_string());
    }
    logger::info("ret");
    // Add instructions to the block.
    //
    block->add(std::move(insn));
    block->add(vm::Ret());
    // Lift basic block.
    //
    block->lifted = lifter->lift_basic_block(block);
    il::optimize_block_function(block->lifted);

    auto slice = lifter->build_function(block->owner, block->vip());
    il::optimize_block_function(slice);

    auto args = lifter->get_return_args(slice);

    args.program_counter()->dump();
    args.return_address()->dump();
    // NOTE: This is not tested and probably wrong.
    //
    if (auto load = llvm::dyn_cast<llvm::LoadInst>(args.program_counter()))
    {
        if (auto gep = llvm::dyn_cast<llvm::GEPOperator>(load->getPointerOperand()))
        {
            if (auto cint = llvm::dyn_cast<llvm::ConstantInt>(gep->getOperand(gep->getNumOperands() - 1)))
            {
                lifter->create_external_call(block->lifted, fmt::format("External.0x{:x}", cint->getLimitedValue()));
                il::optimize_block_function(block->lifted);
            }
        }
    }
    else if (auto cint = llvm::dyn_cast<llvm::ConstantInt>(args.program_counter()))
    {
        lifter->create_external_call(block->lifted, fmt::format("External.0x{:x}", cint->getLimitedValue()));
        il::optimize_block_function(block->lifted);
    }

    if (auto cint = llvm::dyn_cast<llvm::ConstantInt>(args.return_address()))
    {
        auto address = cint->getLimitedValue();
        logger::info("Continue vm execution from 0x{:x}", address);
        tracer = std::make_shared<Tracer>(tracer->getArchitecture());
        tracer->write(tracer->rip_register(), address);
        tracer->write(tracer->rsp_register(), stack_base);

        block->fork(address);
        worklist.push(address);
        snapshots.insert({ address, std::move(tracer) });
    }
    // Terminate current block.
    //
    terminate = true;

    slice->eraseFromParent();
}

void Explorer::operator()(vm::Enter&& insn)
{
    for (const auto& reg : insn.regs())
    {
        logger::info("{:<5} {:<2} {}", "push", reg.size(), reg.op().to_string());
    }
    // Add instructions to the block.
    //
    block->add(std::move(insn));
}

void Explorer::reprove_block()
{
    auto slice = lifter->build_function(block->owner, block->vip());
    il::optimize_block_function(slice);

    auto ret = lifter->get_return_args(slice);

    for (const auto target : il::get_possible_targets(ret.program_counter()))
    {
        if (!block->owner->contains(target))
        {
            logger::info("\tfound new branch: 0x{:x}", target);

            auto fork = tracer->fork();
            auto insn = std::get<vm::Jcc>(tracer->step(step_t::stop_before_branch));

            fork->write(fork->vsp(), target - (insn.direction() == vm::jcc_e::up ? 1 : -1) * 4);
            // Execute branch instruction.
            //
            fork->step(step_t::execute_branch);

            block->fork(target);
            worklist.push(target);
            snapshots.insert({ target, std::move(fork) });
        }
    }

    slice->eraseFromParent();
}

std::set<uint64_t> Explorer::get_reprove_blocks()
{
    std::set<uint64_t> reprove;

    auto fill = [&reprove](const vm::BasicBlock* block, auto&& func) -> void
    {
        for (const auto& child : block->next)
        {
            if (!reprove.contains(child->vip()) && child->next.size() != 2 && child->flow() == vm::flow_t::conditional)
            {
                reprove.insert(child->vip());
                func(child, func);
            }
        }
    };
    fill(block, fill);
    return reprove;
}
