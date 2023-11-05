#include "emulator.hpp"
#include "logger.hpp"

#include <triton/x8664Cpu.hpp>
#include <triton/x86Cpu.hpp>

using namespace triton;
using namespace triton::arch;
using namespace triton::arch::x86;

Emulator::Emulator(triton::arch::architecture_e arch) noexcept
    : Context(arch)
    , image{ std::make_shared<Binary>() }
{
    setMode(modes::MEMORY_ARRAY, false);
    setMode(modes::ALIGNED_MEMORY, true);
    setMode(modes::CONSTANT_FOLDING, true);
    setMode(modes::AST_OPTIMIZATIONS, true);
    setMode(modes::PC_TRACKING_SYMBOLIC, false);
    setMode(modes::TAINT_THROUGH_POINTERS, false);
    setMode(modes::SYMBOLIZE_INDEX_ROTATION, false);

    concretizeAllMemory();
    concretizeAllRegister();

    auto get_memory_cb = [this](triton::Context& context, const triton::arch::MemoryAccess& memory)
    {
        if (!context.isConcreteMemoryValueDefined(memory.getAddress(), memory.getSize()))
        {
            context.setConcreteMemoryAreaValue(memory.getAddress(), this->image->get_bytes(memory.getAddress(), memory.getSize()));
        }
    };

    addCallback(
        triton::callbacks::callback_e::GET_CONCRETE_MEMORY_VALUE,
        triton::callbacks::getConcreteMemoryValueCallback{ get_memory_cb, &get_memory_cb }
    );
}

Emulator::Emulator(Emulator const& other) noexcept
    : Emulator(other.getArchitecture())
{
    for (const auto& [reg_e, reg] : other.getAllRegisters())
        setConcreteRegisterValue(reg, other.getConcreteRegisterValue(reg));

    for (const auto& [addr, value] : other.getConcreteMemory())
        setConcreteMemoryValue(addr, value);
    image = other.image;
}

uint64_t Emulator::read(const triton::arch::Register& reg) const noexcept
{
    return static_cast<uint64_t>(getConcreteRegisterValue(reg));
}

void Emulator::write(const triton::arch::Register& reg, uint64_t value) noexcept
{
    setConcreteRegisterValue(reg, value);
}

uint64_t Emulator::rip() const noexcept
{
    return read(rip_register());
}

uint64_t Emulator::rsp() const noexcept
{
    return read(rsp_register());
}

const triton::arch::Register& Emulator::rip_register() const noexcept
{
    return getRegister(getArchitecture() == arch::ARCH_X86_64 ? "rip" : "eip");
}

const triton::arch::Register& Emulator::rsp_register() const noexcept
{
    return getRegister(getArchitecture() == arch::ARCH_X86_64 ? "rsp" : "esp");
}

uint64_t Emulator::ptrsize() const noexcept
{
    return getArchitecture() == ARCH_X86_64 ? 8 : 4;
}

std::set<triton::arch::Register> Emulator::regs() const noexcept
{
    if (getArchitecture() == triton::arch::ARCH_X86_64)
        return {
            getRegister("rax"),
            getRegister("rbx"),
            getRegister("rcx"),
            getRegister("rdx"),
            getRegister("rdi"),
            getRegister("rsi"),
            getRegister("rsp"),
            getRegister("rbp"),
            getRegister("r8"),
            getRegister("r9"),
            getRegister("r10"),
            getRegister("r11"),
            getRegister("r12"),
            getRegister("r13"),
            getRegister("r14"),
            getRegister("r15"),
        };
    return {
        getRegister("eax"),
        getRegister("ebx"),
        getRegister("ecx"),
        getRegister("edx"),
        getRegister("edi"),
        getRegister("esi"),
        getRegister("esp"),
        getRegister("ebp"),
    };
}

triton::arch::Instruction Emulator::disassemble() const noexcept
{
    auto curr_pc = rip();
    auto bytes   = getConcreteMemoryAreaValue(curr_pc, 16);

    Instruction insn(curr_pc, bytes.data(), bytes.size());
    disassembly(insn);
    return insn;
}

triton::arch::Instruction Emulator::single_step()
{
    auto insn = disassemble();
    execute(insn);
    return insn;
}

void Emulator::execute(triton::arch::Instruction& insn)
{
    if (buildSemantics(insn) != triton::arch::NO_FAULT)
    {
        logger::error("Emulator::execute: Failed to execute instruction at 0x{:x}.", rip());
    }
}
