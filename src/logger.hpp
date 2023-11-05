#pragma once
#include <fmt/core.h>
#include <fmt/color.h>
#include <fmt/chrono.h>
#include <triton/ast.hpp>
#include <triton/context.hpp>

#include <type_traits>
#include <source_location>

#ifdef _MSC_VER
    #include <intrin.h>
    #define unreachable() __assume(0)
#else
    #include <emmintrin.h>
    #include <x86intrin.h>
    #define unreachable() __builtin_unreachable()
#endif

template <>
struct fmt::formatter<triton::ast::SharedAbstractNode> : fmt::formatter<std::string>
{
    template<typename fmtcontext>
    auto format(const triton::ast::SharedAbstractNode& node, fmtcontext& ctx) const
    {
        std::stringstream ss;
        ss << node;
        return formatter<std::string>::format(ss.str(), ctx);
    }
};

template<>
struct fmt::formatter<triton::engines::symbolic::SharedSymbolicVariable> : fmt::formatter<std::string>
{
    template<typename fmtcontext>
    auto format(const triton::engines::symbolic::SharedSymbolicVariable& var, fmtcontext& ctx) const
    {
        auto alias = var->getAlias();
        return formatter<std::string>::format(alias.empty() ? var->getName() : alias, ctx);
    }
};

template<>
struct fmt::formatter<triton::engines::symbolic::SharedSymbolicExpression> : fmt::formatter<std::string>
{
    template<typename fmtcontext>
    auto format(const triton::engines::symbolic::SharedSymbolicExpression& expr, fmtcontext& ctx) const
    {
        return formatter<std::string>::format(expr->getFormattedExpression(), ctx);
    }
};

template<>
struct fmt::formatter<triton::arch::Instruction> : fmt::formatter<std::string>
{
    template<typename fmtcontext>
    auto format(const triton::arch::Instruction& ins, fmtcontext& ctx) const
    {
        return formatter<std::string>::format(fmt::format("0x{:x} {}", ins.getAddress(), ins.getDisassembly()), ctx);
    }
};

template<>
struct fmt::formatter<triton::arch::Register> : fmt::formatter<std::string>
{
    template<typename fmtcontext>
    auto format(const triton::arch::Register& reg, fmtcontext& ctx) const
    {
        return formatter<std::string>::format(reg.getName(), ctx);
    }
};

namespace logger
{
static void debug(const char* format, auto... args)
{
    fmt::print(fg(fmt::color::dark_orange) | fmt::emphasis::bold, "[DEBUG] ");
    fmt::print(fmt::runtime(format), args...);
    fmt::print("\n");
}

static void info(const char* format, const auto&... args)
{
    fmt::print(fg(fmt::color::cadet_blue) | fmt::emphasis::bold, "[INFO]  ");
    fmt::print(fmt::runtime(format), args...);
    fmt::print("\n");
}

static void warn(const char* format, const auto&... args)
{
    fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "[WARN]  ");
    fmt::print(fmt::runtime(format), args...);
    fmt::print("\n");
}

static void error [[noreturn]](const char* format, const auto&... args)
{
    fmt::print(fg( fmt::color::red) | fmt::emphasis::bold, "[ERROR] ");
    fmt::print(fmt::runtime(format), args...);
    fmt::print("\n");
    // Never return.
    //
    unreachable();
}
};
