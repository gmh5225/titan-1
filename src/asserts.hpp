#pragma once
#include <stdexcept>

inline static constexpr void abort_if(bool condition, const char* string)
{
    if (condition)
    {
        throw std::logic_error{ string };
    }
}

#define fassert__stringify(x) #x
#define fassert__istringify(x) fassert__stringify(x)
#define fassert(...) abort_if(!bool(__VA_ARGS__), fassert__stringify(__VA_ARGS__) " at " __FILE__ ":" fassert__istringify(__LINE__))
