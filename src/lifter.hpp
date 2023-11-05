#pragma once
#include "vm/routine.hpp"
#include "logger.hpp"

#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>

struct ReturnArguments
{
    explicit ReturnArguments(llvm::Value* rip, llvm::Value* ret) : rip{ rip }, ret{ ret } {}

    llvm::Value* return_address()  const noexcept;
    llvm::Value* program_counter() const noexcept;

private:
    llvm::Value* rip;
    llvm::Value* ret;
};

// IR to LLVM Lifter class.
//
struct Lifter
{
    Lifter();

    // Lift `basic_block` into llvm function.
    // All basic_blocks represented as llvm::Function's.
    //
    llvm::Function* lift_basic_block(vm::BasicBlock* block);

    // Build paritual or full control flow graph of a routine.
    //
    llvm::Function* build_function(const vm::Routine* routine, uint64_t target_block = vm::invalid_vip);

    // Get program counter and [rsp] value from KeepReturn function.
    //
    ReturnArguments get_return_args(llvm::Function* function) const;

    void create_external_call(llvm::Function* function, const std::string& name);

    void operator()(const vm::Add&);
    void operator()(const vm::Shl&);
    void operator()(const vm::Shr&);
    void operator()(const vm::Ldr&);
    void operator()(const vm::Str&);
    void operator()(const vm::Nor&);
    void operator()(const vm::Nand&);
    void operator()(const vm::Shrd&);
    void operator()(const vm::Shld&);
    void operator()(const vm::Push&);
    void operator()(const vm::Pop&);
    void operator()(const vm::Jmp&);
    void operator()(const vm::Ret&);
    void operator()(const vm::Jcc&);
    void operator()(const vm::Exit&);
    void operator()(const vm::Enter&);

private:
    llvm::Value* create_memory_read_64(llvm::Value* address);
    llvm::Value* create_memory_write_64(llvm::Value* address, llvm::Value* ptr);

    std::vector<llvm::BasicBlock*> get_exit_blocks(llvm::Function* function) const;

    // Get virtual instruction pointer from function arguments.
    //
    llvm::Argument* vip();

    // Get virtual stack pointer from function arguments.
    //
    llvm::Argument* vsp();

    // Get virtual registers array from function arguments.
    //
    llvm::Argument* vregs();

    // Get llvm function for vmp instruction.
    //
    llvm::Function* sem(const std::string& name) const;

    // Get function argument by name.
    //
    llvm::Argument* arg(llvm::Function* fn, const std::string& name);

    // Get function argument by name.
    //
    llvm::Argument* arg(const std::string& name);

    // Shallow copy function into a new one.
    // The global variables which the function accesses will not be copied.
    //
    llvm::Function* clone(llvm::Function* fn);

    // Wrap `fn` with helper_slice_fn function.
    //
    llvm::Function* make_slice(llvm::Function* fn);

    // Wrap `fn` with helper_lifted_fn function.
    //
    llvm::Function* make_final(llvm::Function* fn, uint64_t vip);

    // Current basic block function that is being lifted.
    //
    llvm::Function* function;

    //
    //
    llvm::IRBuilder<> ir;

    // Resolved semantics from intrinsics module and their functions.
    //
    std::unordered_map<std::string, llvm::Function*> sems;

    // Helpers loaded from intrinsics file.
    //
    llvm::Function* helper_lifted_fn;
    llvm::Function* helper_empty_block_fn;
    llvm::Function* helper_block_fn;
    llvm::Function* helper_slice_fn;
    llvm::Function* helper_keep_fn;
    llvm::Value*    helper_undef;

    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
};
