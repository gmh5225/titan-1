#pragma once

#include <triton/ast.hpp>
#include <triton/instruction.hpp>
#include <triton/symbolicEngine.hpp>
#include <triton/x86Specifications.hpp>

#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/find_if.hpp>

#include <llvm/IR/Function.h>
#include <optional>

bool has_variable(const auto& range, const std::string& alias)
{
    return ranges::any_of(range, [&](const auto& node) { return node->getAlias() == alias; });
}

bool has_variable(const auto& range, const std::string& alias, const auto&... args)
{
    return range.size() == 1 + sizeof...(args) && has_variable(range, alias) && has_variable(range, args...);
}

auto get_variable(const auto& range, const std::string& alias) -> std::optional<triton::engines::symbolic::SharedSymbolicVariable>
{
    auto var = ranges::find_if(range, [&](const auto& var){ return var->getAlias() == alias; });
    if (var != ranges::end(range))
        return *var;
    return {};
}

auto collect_variables(const triton::ast::SharedAbstractNode& ast) -> std::vector<triton::engines::symbolic::SharedSymbolicVariable>;
bool is_variable(const triton::ast::SharedAbstractNode& node, const std::string& alias = "");
auto to_variable(const triton::ast::SharedAbstractNode& node) -> triton::engines::symbolic::SharedSymbolicVariable;

// Matches mov/movzx/movsx register, register.
//
bool op_mov_register_register(const triton::arch::Instruction& insn);

// Matches mov/movzx/movsx [memory], register.
//
bool op_mov_memory_register(const triton::arch::Instruction& insn);

// Matches mov/movzx/movsx register, [memory].
//
bool op_mov_register_memory(const triton::arch::Instruction& insn);

// Matches pop register.
//
bool op_pop_register(const triton::arch::Instruction& insn);

// Matches jmp register.
//
bool op_jmp_register(const triton::arch::Instruction& insn);

// Matches popfq/popfd.
//
bool op_pop_flags(const triton::arch::Instruction& insn);

// Matches `lea register, [rip - 7]`
//
bool op_lea_rip(const triton::arch::Instruction& insn);

// Matches ret.
//
bool op_ret(const triton::arch::Instruction& insn);

void save_ir(llvm::Value* value,   const std::string& filename);
void save_ir(llvm::Module* module, const std::string& filename);
