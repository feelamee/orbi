#pragma once

#include <source_location>
#include <type_traits>

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

template <class Enum>
constexpr std::underlying_type_t<Enum>
to_underlying(Enum e) noexcept
{
    return static_cast<std::underlying_type_t<Enum>>(e);
}

struct noncopyable
{
    noncopyable(noncopyable const&) = delete;
    noncopyable& operator=(noncopyable const&) = delete;
};

} // namespace orbi::detail
