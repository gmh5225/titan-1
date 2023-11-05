#pragma once

#include "binary.hpp"

#include <functional>
#include <optional>

#include <triton/context.hpp>
#include <triton/basicBlock.hpp>
#include <triton/x86Specifications.hpp>

struct Emulator : public triton::Context
{
    explicit Emulator(triton::arch::architecture_e arch) noexcept;

    Emulator(Emulator const& other) noexcept;

    uint64_t ptrsize() const noexcept;
    // Most used registers getters.
    //
    uint64_t rip() const noexcept;
    uint64_t rsp() const noexcept;

    const triton::arch::Register& rip_register() const noexcept;
    const triton::arch::Register& rsp_register() const noexcept;

    std::set<triton::arch::Register> regs() const noexcept;

    uint64_t read(const triton::arch::Register& reg) const noexcept;
    template<typename T>
    T read(uint64_t address) const noexcept
    {
        return static_cast<T>(getConcreteMemoryValue({ address, sizeof(T) }));
    }
    template<typename T>
    T read(const triton::arch::MemoryAccess& memory) const noexcept
    {
        return static_cast<T>(getConcreteMemoryValue(memory));
    }

    void write(const triton::arch::Register& reg, uint64_t value) noexcept;
    template<typename T>
    void write(uint64_t address, T value) noexcept
    {
        setConcreteMemoryValue({ address, sizeof(T) }, value);
    }
    template<typename T>
    void write(const triton::arch::MemoryAccess& memory, T value) const noexcept
    {
        return static_cast<T>(setConcreteMemoryValue(memory, value));
    }

    triton::arch::Instruction disassemble() const noexcept;
    triton::arch::Instruction single_step();

    void execute(triton::arch::Instruction& insn);

protected:
    std::shared_ptr<Binary> image;
};
