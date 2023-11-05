#include "routine.hpp"
#include "asserts.hpp"
#include "logger.hpp"

namespace vm
{
BasicBlock::BasicBlock(uint64_t vip, Routine* rtn)
    : vip_(vip), owner(rtn), lifted(nullptr)
{
    owner->blocks.emplace(vip, this);
}

BasicBlock* BasicBlock::fork(uint64_t vip)
{
    if (owner->contains(vip))
    {
        next.push_back(owner->blocks.at(vip));
        return nullptr;
    }

    auto block = new BasicBlock(vip, owner);

    next.push_back(block);
    return block;
}

void BasicBlock::add(Instruction&& insn) noexcept
{
    vins.push_back(std::move(insn));
}

uint64_t BasicBlock::vip() const noexcept
{
    return vip_;
}

flow_t BasicBlock::flow() const noexcept
{
    if (vins.empty())
        return flow_t::unknown;
    else if (op_exit(vins.back()))
        return flow_t::exit;
    else if (op_jcc(vins.back()))
        return flow_t::conditional;
    else if (op_jmp(vins.back()))
        return flow_t::unconditional;
    return flow_t::unknown;
}

Routine::Routine(uint64_t vip)
{
    entry = new BasicBlock(vip, this);
}

Routine::~Routine()
{
    for (auto& [vip, block] : blocks)
        delete block;
}

BasicBlock* Routine::begin(uint64_t vip)
{
    return (new Routine(vip))->entry;
}

bool Routine::contains(uint64_t vip) const noexcept
{
    return blocks.count(vip) > 0;
}

std::string Routine::dot() const noexcept
{
    std::string body = "digraph g {\n";

    for (const auto& [vip, block] : blocks)
    {
        for (const auto& next : block->next)
            body += fmt::format("vip_0x{:08x} -> vip_0x{:08x} []\n", vip, next->vip());
    }
    return body + "}\n";
}
}
