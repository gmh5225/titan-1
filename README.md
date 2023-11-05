# titan - VMProtect devirtualizer

I'm releasing my VMProtect devirtualizer for others to research, learn, and improve. This project started in 2018 as a hobby project and was rewritten at least 4 times. During my research, I've met with awesome 
people, made friends, and learned a lot.

## How does it work?

The tool uses [Triton](https://github.com/JonathanSalwan/Triton) for emulation, symbolic execution, and lifting. The easiest way to match VM handlers is to match them on the Triton AST level. The tool symbolizes vip and vsp registers and propagates memory loads and stores. Almost every handler ends with the store (to the stack, vm register or memory). We take Triton AST of the value that is being stored and match against known patterns:
```c
// Match [vsp] + [vsp].
//
static bool match_add(const triton::ast::SharedAbstractNode& ast)
{
    if (ast->getType() == triton::ast::EXTRACT_NODE)
    {
        return match_add(ast->getChildren()[2]->getChildren()[1]);
    }
    return ast->getType() == triton::ast::BVADD_NODE
        && is_variable(ast->getChildren()[1], variable::vsp_fetch);
}
```

No matter how obfuscated handlers are, it is possible to match them with a single x86 instruction! Once the handler is identified, it is lifted into a basic block. Once the basic block is terminated, the partial control-flow graph is computed and the RIP register is sliced, giving the address of the next basic block. The process repeats until no new basic blocks are found.
Every basic block is lifted into separate LLVM function. The process of building control-flow graph comes down chaining calls to basic block functions in the right order.
The tool has few custom LLVM passes like `no-alias` and `memory coalescing` passes. The only pass that is left to implement is `flag synthesis` pass which will give the cleanest LLVM bitcode.

## Usage

The tool requires 3 arguments:
- Path to vmprotect intrinsics file
- Path to virtualized binary
- Virtual address of vm entry point
```
./build/titan
titan: for the -i option: must be specified at least once!
titan: for the -b option: must be specified at least once!
titan: for the -e option: must be specified at least once!
./build/titan -i intrinsics/vmprotect64.ll -b samples/loop_hash.0x140103FF4.exe -e 0x140103FF4
```

## Acknowledgements

_Matteo Favaro_ and _Vlad Malagar_ for answering my sometimes dumb questions, helping to find bugs in llvm bitcode, giving motivation and new ideas.