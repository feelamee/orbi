#pragma once

#include <format>
#include <stdexcept>

namespace orbi
{

struct runtime_error : std::runtime_error
{
    template <class... Args>
    runtime_error(std::format_string<Args...> fmt, Args&&... args)
        : std::runtime_error{ std::format(std::move(fmt), std::forward<Args>(args)...) }
    {
    }
};

} // namespace orbi
