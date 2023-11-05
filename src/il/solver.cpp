#include "asserts.hpp"
#include "solver.hpp"
#include "logger.hpp"
#include "utils.hpp"

#include <fstream>

#include <llvm/IR/Module.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instruction.h>

#include <triton/llvmToTriton.hpp>

llvm::cl::opt<bool> save_branch_ast("solver-save-ast",
    llvm::cl::desc("Save branch ast into dot file on every branch."),
    llvm::cl::value_desc("flag"),
    llvm::cl::init(false),
    llvm::cl::Optional);

llvm::cl::opt<bool> print_branch_ast("solver-print-ast",
    llvm::cl::desc("Print branch ast on every branch."),
    llvm::cl::value_desc("flag"),
    llvm::cl::init(false),
    llvm::cl::Optional);

namespace il
{
std::vector<uint64_t> get_possible_targets(llvm::Value* ret)
{
    if (ret == nullptr)
    {
        logger::error("get_possible_targets argument got null argument.");
    }

    std::vector<uint64_t> targets;
    // Does not matter which arch we use.
    //
    triton::Context api(triton::arch::ARCH_X86_64);
    api.setAstRepresentationMode(triton::ast::representations::SMT_REPRESENTATION);

    if (auto inst = llvm::dyn_cast<llvm::Instruction>(ret))
    {
        if (inst->getOpcode() == llvm::Instruction::Or)
        {
            logger::warn("replacing or with add.");
            llvm::IRBuilder<> ir(inst);
            ret = ir.CreateAdd(inst->getOperand(0), inst->getOperand(1));
            inst->replaceAllUsesWith(ret);
        }
    }
    triton::ast::LLVMToTriton lifter(api);
    // Lift llvm into triton ast.
    //
    auto node = lifter.convert(ret);

    if (save_branch_ast)
    {
        static int solver_temp_names;
        std::fstream fd(fmt::format("branch-ast-{}.dot", solver_temp_names++), std::ios_base::out);
        if (fd.good())
            api.liftToDot(fd, node);
    }
    if (print_branch_ast)
    {
        logger::info("branch ast: {}", triton::ast::unroll(node));
    }
    // If constant.
    //
    if (!node->isSymbolized())
    {
        return { static_cast<uint64_t>(node->evaluate()) };
    }
    auto ast         = api.getAstContext();
    auto zero        = ast->bv(0, node->getBitvectorSize());
    auto constraints = ast->distinct(node, zero);

    while (true)
    {
        // Failsafe.
        //
        if (targets.size() > 2)
            return {};

        auto model = api.getModel(constraints);

        if (model.empty())
            break;
        for (auto& [id, sym] : model)
            api.setConcreteVariableValue(api.getSymbolicVariable(id), sym.getValue());

        auto target = static_cast<uint64_t>(node->evaluate());
        targets.push_back(target);
        // Update constraints.
        //
        constraints = ast->land(constraints, ast->distinct(node, ast->bv(target, node->getBitvectorSize())));
    }
    return targets;
}
};
