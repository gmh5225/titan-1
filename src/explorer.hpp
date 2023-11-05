#pragma once

#include "vm/routine.hpp"

#include "lifter.hpp"
#include "tracer.hpp"

#include <stack>

struct Explorer
{
    Explorer(std::shared_ptr<Lifter> lifter, std::shared_ptr<Tracer> tracer);

    std::unique_ptr<vm::Routine> explore(uint64_t address);

    void operator()(vm::Add&&);
    void operator()(vm::Shl&&);
    void operator()(vm::Shr&&);
    void operator()(vm::Ldr&&);
    void operator()(vm::Str&&);
    void operator()(vm::Nor&&);
    void operator()(vm::Nand&&);
    void operator()(vm::Shld&&);
    void operator()(vm::Shrd&&);
    void operator()(vm::Push&&);
    void operator()(vm::Pop&&);
    void operator()(vm::Jmp&&);
    void operator()(vm::Ret&&);
    void operator()(vm::Jcc&&);
    void operator()(vm::Exit&&);
    void operator()(vm::Enter&&);

private:
    void reprove_block();

    std::set<uint64_t> get_reprove_blocks();

    // LLVM Lifter instance.
    //
    std::shared_ptr<Lifter> lifter;

    // Emulator that is currently active.
    //
    std::shared_ptr<Tracer> tracer;

    // List of blocks to explore.
    //
    std::stack<uint64_t> worklist;

    // List of blocks already explored.
    //
    std::set<uint64_t> explored;

    // Saved snapshots for every basic block.
    //
    std::map<uint64_t, std::shared_ptr<Tracer>> snapshots;

    // Block that is currently processing.
    //
    vm::BasicBlock* block;

    // Terminate block exploration loop.
    //
    bool terminate;
};
