#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <vector>

std::vector<std::byte>
read_file(std::filesystem::path const& filename)
{
    std::ifstream file(filename, std::ios::binary);
    file.exceptions(std::ifstream::badbit);

    auto const size{ std::filesystem::file_size(filename) };
    std::vector<std::byte> buffer{ size };

    file.read(reinterpret_cast<std::ifstream::char_type*>(buffer.data()), static_cast<std::streamsize>(size));

    return buffer;
}

void
handle_nested_exceptions(std::exception const& e, int level = 0)
{
    if (level == 0)
    {
        std::cerr << "Exception was thrown:\n";
        ++level;
    }

    for (auto _ : std::views::iota(0, level)) std::cerr << "    ";
    std::cerr << "what(): " << e.what() << '\n';

    try
    {
        std::rethrow_if_nested(e);
    }
    catch (std::exception const& nested)
    {
        handle_nested_exceptions(nested, level + 1);
    }
}

int
main()
try
{
}
catch (std::exception const& e)
{
    handle_nested_exceptions(e);
}
