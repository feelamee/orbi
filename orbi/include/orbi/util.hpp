#pragma once

#include <source_location>

namespace orbi::detail
{

[[noreturn]] void unimplemented(std::source_location location = std::source_location::current());

[[noreturn]] inline void
unreachable()
{
    // Uses compiler specific extensions if possible.
    // Even if no extension is used, undefined behavior is still raised by
    // an empty function body and the noreturn attribute.
#if defined(_MSC_VER) && !defined(__clang__) // MSVC
    __assume(false);
#else // GCC, Clang
    __builtin_unreachable();
#endif
}

} // namespace orbi::detail
