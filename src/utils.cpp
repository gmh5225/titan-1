#include "utils.hpp"

#include <llvm/IR/Module.h>

std::vector<triton::engines::symbolic::SharedSymbolicVariable> collect_variables(const triton::ast::SharedAbstractNode& ast)
{
    using namespace triton::ast;
    const auto vec = childrenExtraction(ast, true, true);
    return vec
        | ranges::views::filter   ([](const SharedAbstractNode& node){ return node->getType() == VARIABLE_NODE; })
        | ranges::views::transform([](const SharedAbstractNode& node){ return std::dynamic_pointer_cast<VariableNode>(node)->getSymbolicVariable(); })
        | ranges::to_vector;
}

bool is_variable(const triton::ast::SharedAbstractNode& node, const std::string& alias)
{
    if (alias.empty())
        return node->getType() == triton::ast::VARIABLE_NODE;
    return node->getType() == triton::ast::VARIABLE_NODE
        && std::dynamic_pointer_cast<triton::ast::VariableNode>(node)->getSymbolicVariable()->getAlias() == alias;
}

triton::engines::symbolic::SharedSymbolicVariable to_variable(const triton::ast::SharedAbstractNode& node)
{
    return std::dynamic_pointer_cast<triton::ast::VariableNode>(node)->getSymbolicVariable();
}

bool op_mov_register_register(const triton::arch::Instruction& insn)
{
    return (insn.getType() == triton::arch::x86::ID_INS_MOV
        ||  insn.getType() == triton::arch::x86::ID_INS_MOVZX
        ||  insn.getType() == triton::arch::x86::ID_INS_MOVSX)
        &&  insn.operands[0].getType() == triton::arch::OP_REG
        &&  insn.operands[1].getType() == triton::arch::OP_REG;
}

bool op_mov_register_memory(const triton::arch::Instruction& insn)
{
    return (insn.getType() == triton::arch::x86::ID_INS_MOV
        ||  insn.getType() == triton::arch::x86::ID_INS_MOVZX
        ||  insn.getType() == triton::arch::x86::ID_INS_MOVSX)
        &&  insn.operands[0].getType() == triton::arch::OP_REG
        &&  insn.operands[1].getType() == triton::arch::OP_MEM;
}

bool op_mov_memory_register(const triton::arch::Instruction& insn)
{
    return (insn.getType() == triton::arch::x86::ID_INS_MOV
        ||  insn.getType() == triton::arch::x86::ID_INS_MOVZX
        ||  insn.getType() == triton::arch::x86::ID_INS_MOVSX)
        &&  insn.operands[0].getType() == triton::arch::OP_MEM
        &&  insn.operands[1].getType() == triton::arch::OP_REG;
}

bool op_pop_register(const triton::arch::Instruction& insn)
{
    return insn.getType() == triton::arch::x86::ID_INS_POP
        && insn.operands[0].getType() == triton::arch::OP_REG;
}

bool op_jmp_register(const triton::arch::Instruction& insn)
{
    return insn.getType() == triton::arch::x86::ID_INS_JMP
        && insn.operands[0].getType() == triton::arch::OP_REG;
}

bool op_pop_flags(const triton::arch::Instruction& insn)
{
    return insn.getType() == triton::arch::x86::ID_INS_POPFQ || insn.getType() == triton::arch::x86::ID_INS_POPFD;
}

bool op_lea_rip(const triton::arch::Instruction& insn)
{
    return insn.getType() == triton::arch::x86::ID_INS_LEA
        && insn.operands[1].getConstMemory().getConstBaseRegister().getId() == triton::arch::ID_REG_X86_RIP
        && insn.operands[1].getConstMemory().getConstDisplacement().getValue() == -7;
}

bool op_ret(const triton::arch::Instruction& insn)
{
    return insn.getType() == triton::arch::x86::ID_INS_RET;
}

void save_ir(llvm::Value* value, const std::string& filename)
{
    std::error_code ec;
    llvm::raw_fd_ostream fd(filename, ec);
    value->print(fd, false);
}

void save_ir(llvm::Module* module, const std::string& filename)
{
    std::error_code ec;
    llvm::raw_fd_ostream fd(filename, ec);
    module->print(fd, nullptr);
}
