#pragma once

#include <optional>
#include <variant>
#include <vector>
#include <string>

struct Tracer;

namespace vm
{
enum class jcc_e
{
    up,
    down
};

struct PhysicalRegister
{
    explicit PhysicalRegister(std::string name);

    const std::string& name() const noexcept;

private:
    std::string name_;
};

struct VirtualRegister
{
    explicit VirtualRegister(int number, int offset);

    int number() const noexcept;
    int offset() const noexcept;

private:
    int number_;
    int offset_;
};

struct VirtualStackPointer
{
};

struct Immediate
{
    explicit Immediate(uint64_t value);

    uint64_t value() const noexcept;

private:
    uint64_t value_;
};

struct Operand
{
    Operand(Immediate&& imm) noexcept;
    Operand(VirtualRegister&& reg) noexcept;
    Operand(PhysicalRegister&& reg) noexcept;
    Operand(VirtualStackPointer&& reg) noexcept;

    bool is_vsp() const noexcept;
    bool is_virtual() const noexcept;
    bool is_physical() const noexcept;
    bool is_immediate() const noexcept;

    const Immediate& imm() const noexcept;
    const VirtualRegister& vrt() const noexcept;
    const PhysicalRegister& phy() const noexcept;
    const VirtualStackPointer& vsp() const noexcept;

    std::string to_string() const noexcept;

private:
    std::variant<PhysicalRegister, VirtualRegister, VirtualStackPointer, Immediate> op;
};

struct Sized
{
    Sized(int size);

    int size() const noexcept;

protected:
    int size_;
};

struct Add : public Sized
{
    using Sized::Sized;
};

struct Shl : public Sized
{
    using Sized::Sized;
};

struct Shr : public Sized
{
    using Sized::Sized;
};

struct Shrd : public Sized
{
    using Sized::Sized;
};

struct Shld : public Sized
{
    using Sized::Sized;
};

struct Ldr : public Sized
{
    using Sized::Sized;
};

struct Str : public Sized
{
    using Sized::Sized;
};

struct Nor : public Sized
{
    using Sized::Sized;
};

struct Nand : public Sized
{
    using Sized::Sized;
};

struct Push : public Sized
{
    Push(Operand&& operand, int size);

    const Operand& op() const noexcept;

private:
    Operand operand;
};

struct Pop : public Sized
{
    Pop(Operand&& operand, int size);

    const Operand& op() const noexcept;

private:
    Operand operand;
};

struct Jmp{};
struct Ret{};
struct Exit
{
    Exit(std::vector<Pop> context);

    const std::vector<Pop>& regs() const noexcept;

private:
    std::vector<Pop> context;
};

struct Enter
{
    Enter(std::vector<Push> context);

    const std::vector<Push>& regs() const noexcept;

private:
    std::vector<Push> context;
};

struct Jcc
{
    Jcc(jcc_e type, std::string vip, std::string vsp);

    const std::string& vip_register() const noexcept;
    const std::string& vsp_register() const noexcept;
    jcc_e              direction()    const noexcept;

private:
    jcc_e type;
    std::string vip;
    std::string vsp;
};

using Instruction = std::variant<Add, Nor, Nand, Shl, Shr, Shrd, Shld, Ldr, Str, Push, Pop, Jmp, Ret, Exit, Enter, Jcc>;

bool op_push_imm(const Instruction& insn);
bool op_branch(const Instruction& insn);
bool op_enter(const Instruction& insn);
bool op_exit(const Instruction& insn);
bool op_pop(const Instruction& insn);
bool op_jmp(const Instruction& insn);
bool op_jcc(const Instruction& insn);
};
