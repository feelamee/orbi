#include <orbi/detail/util.hpp>

#include <format>
#include <iostream>

namespace orbi::detail
{

[[noreturn]] void
unimplemented(std::source_location location)
{
    std::cout << std::format("{}:{}:{}: in {}\n\tunimplemented...", location.file_name(),
                             location.line(), location.column(), location.function_name())
              << std::endl;
    std::abort();
}

} // namespace orbi::detail
