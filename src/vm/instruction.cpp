#include "instruction.hpp"
#include "logger.hpp"

namespace vm
{
PhysicalRegister::PhysicalRegister(std::string name)
    : name_{ std::move(name) }
{
}

const std::string& PhysicalRegister::name() const noexcept
{
    return name_;
}

VirtualRegister::VirtualRegister(int number, int offset)
    : number_{ number }, offset_{ offset }
{
}

int VirtualRegister::number() const noexcept
{
    return number_;
}

int VirtualRegister::offset() const noexcept
{
    return offset_;
}

Immediate::Immediate(uint64_t value)
    : value_{ value }
{
}

uint64_t Immediate::value() const noexcept
{
    return value_;
}

Operand::Operand(Immediate&& imm)           noexcept : op{ std::move(imm) } {}
Operand::Operand(VirtualRegister&& reg)     noexcept : op{ std::move(reg) } {}
Operand::Operand(PhysicalRegister&& reg)    noexcept : op{ std::move(reg) } {}
Operand::Operand(VirtualStackPointer&& reg) noexcept : op{ std::move(reg) } {}

bool Operand::is_vsp() const noexcept
{
    return std::holds_alternative<VirtualStackPointer>(op);
}
bool Operand::is_virtual() const noexcept
{
    return std::holds_alternative<VirtualRegister>(op);
}
bool Operand::is_physical() const noexcept
{
    return std::holds_alternative<PhysicalRegister>(op);
}
bool Operand::is_immediate() const noexcept
{
    return std::holds_alternative<Immediate>(op);
}

const Immediate& Operand::imm() const noexcept
{
    return std::get<Immediate>(op);
}
const VirtualRegister& Operand::vrt() const noexcept
{
    return std::get<VirtualRegister>(op);
}
const PhysicalRegister& Operand::phy() const noexcept
{
    return std::get<PhysicalRegister>(op);
}
const VirtualStackPointer& Operand::vsp() const noexcept
{
    return std::get<VirtualStackPointer>(op);
}

std::string Operand::to_string() const noexcept
{
    if (is_immediate())
        return fmt::format("0x{:016x}", imm().value());
    else if (is_physical())
        return fmt::format("{}", phy().name());
    else if (is_virtual())
        return fmt::format("vmregs[{:02}:{:02}]", vrt().number(), vrt().offset());
    return fmt::format("{}", "vsp");
}

Sized::Sized(int size)
    : size_{ size }
{
}

int Sized::size() const noexcept
{
    return size_;
}

Push::Push(Operand&& operand, int size)
    : Sized(size), operand{ std::move(operand) }
{
}

const Operand& Push::op() const noexcept
{
    return operand;
}

Pop::Pop(Operand&& operand, int size)
    : Sized(size), operand{ std::move(operand) }
{
}

const Operand& Pop::op() const noexcept
{
    return operand;
}

Exit::Exit(std::vector<Pop> context)
    : context(std::move(context))
{
}

const std::vector<Pop>& Exit::regs() const noexcept
{
    return context;
}

Enter::Enter(std::vector<Push> context)
    : context(std::move(context))
{
}

const std::vector<Push>& Enter::regs() const noexcept
{
    return context;
}

Jcc::Jcc(jcc_e type, std::string vip, std::string vsp)
    : type{ type }, vip{ std::move(vip) }, vsp{ std::move(vsp) }
{
}

const std::string& Jcc::vip_register() const noexcept
{
    return vip;
}

const std::string& Jcc::vsp_register() const noexcept
{
    return vsp;
}

jcc_e Jcc::direction() const noexcept
{
    return type;
}

bool op_push_imm(const Instruction& insn)
{
    return std::holds_alternative<Push>(insn)
        && std::get<Push>(insn).op().is_immediate();
}

bool op_branch(const Instruction& insn)
{
    return std::holds_alternative<Jmp>(insn)
        || std::holds_alternative<Jcc>(insn)
        || std::holds_alternative<Exit>(insn);
}

bool op_enter(const Instruction& insn)
{
    return std::holds_alternative<Enter>(insn);
}

bool op_exit(const Instruction& insn)
{
    return std::holds_alternative<Exit>(insn);
}

bool op_pop(const Instruction& insn)
{
    return std::holds_alternative<Pop>(insn);
}

bool op_jmp(const Instruction& insn)
{
    return std::holds_alternative<Jmp>(insn);
}

bool op_jcc(const Instruction& insn)
{
    return std::holds_alternative<Jcc>(insn);
}
};
