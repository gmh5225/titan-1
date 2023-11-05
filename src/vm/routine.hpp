#pragma once

#include "instruction.hpp"

#include <set>
#include <string>
#include <vector>
#include <unordered_map>

namespace llvm
{
class Function;
}

namespace vm
{
static constexpr auto invalid_vip = ~0ull;

enum class flow_t
{
    exit,
    unknown,
    conditional,
    unconditional,
};

struct Routine;

struct BasicBlock
{
    BasicBlock(uint64_t vip, Routine* rtn);

    // Fork a new block from this block and link them together.
    //
    BasicBlock* fork(uint64_t vip);

    void add(Instruction&& insn) noexcept;

    auto begin() const noexcept { return vins.begin(); }
    auto end()   const noexcept { return vins.end();   }

    uint64_t vip() const noexcept;
    flow_t flow()  const noexcept;

    // Routine to which this block belongs to.
    //
    Routine* owner;

    llvm::Function* lifted;

    std::vector<BasicBlock*> next;

private:
    uint64_t vip_;

    std::vector<Instruction> vins;
};

struct Routine
{
    Routine(uint64_t vip);
    ~Routine();

    // Create a routine and return entry basic block.
    //
    static BasicBlock* begin(uint64_t vip);

    bool contains(uint64_t vip) const noexcept;

    // Build graphviz control-flow graph.
    //
    std::string dot() const noexcept;

    auto begin() const noexcept { return blocks.begin(); }
    auto end()   const noexcept { return blocks.end();   }

    // First block in this routine.
    //
    BasicBlock* entry;

    // Explored blocks.
    //
    std::unordered_map<uint64_t, BasicBlock*> blocks;
};
}
