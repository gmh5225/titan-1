#include "il/optimizer.hpp"
#include "explorer.hpp"
#include "emulator.hpp"
#include "logger.hpp"
#include "binary.hpp"
#include "utils.hpp"

#include <llvm/Support/Signals.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/PrettyStackTrace.h>

#include <fstream>

llvm::cl::opt<uint64_t> entrypoint("e",
    llvm::cl::desc("Virtual address of vmenter"),
    llvm::cl::value_desc("entrypoint"),
    llvm::cl::Required);

llvm::cl::opt<std::string> output("o",
    llvm::cl::desc("Path to the output .ll file"),
    llvm::cl::value_desc("output"),
    llvm::cl::init("output.ll"),
    llvm::cl::Optional);

// Necessary command line optimization flags for llvm. Thank you Matteo.
//
static const std::vector<const char*> optimization_args =
{
    "-memdep-block-scan-limit=100000",
    "-rotation-max-header-size=100000",
    "-earlycse-mssa-optimization-cap=1000000",
    "-dse-memoryssa-defs-per-block-limit=1000000",
    "-dse-memoryssa-partial-store-limit=1000000",
    "-dse-memoryssa-path-check-limit=1000000",
    "-dse-memoryssa-scanlimit=1000000",
    "-dse-memoryssa-walklimit=1000000",
    "-dse-memoryssa-otherbb-cost=2",
    "-memssa-check-limit=1000000",
    "-memdep-block-number-limit=1000000",
    "-memdep-block-scan-limit=1000000",
    "-gvn-max-block-speculations=1000000",
    "-gvn-max-num-deps=1000000",
    "-gvn-hoist-max-chain-length=-1",
    "-gvn-hoist-max-depth=-1",
    "-gvn-hoist-max-bbs=-1",
    "-unroll-threshold=1000000"
};

int main(int argc, char** argv)
{
    // Inject optimization options to argv.
    //
    std::vector<const char*> args;
    std::copy_n(argv, argc, std::back_inserter(args));
    std::copy(optimization_args.begin(), optimization_args.end(), std::back_inserter(args));
    // Enable stack traces.
    //
    llvm::sys::PrintStackTraceOnErrorSignal(args[0]);
    llvm::PrettyStackTraceProgram X(args.size(), args.data());
    // Parse command parameters.
    //
    llvm::cl::ParseCommandLineOptions(args.size(), args.data());

    auto lifter = std::make_shared<Lifter>();
    auto tracer = std::make_shared<Tracer>(triton::arch::architecture_e::ARCH_X86_64);
    Explorer explorer(lifter, tracer);

    auto rtn = explorer.explore(entrypoint);
    auto fn  = lifter->build_function(rtn.get());

    il::optimize_virtual_function(fn);

    save_ir(fn, fmt::format("function.{}", output));

    return 0;
}
