#include <orbi/detail/util.hpp>
#include <orbi/semver.hpp>

#include <charconv>
#include <format>
#include <ranges>

namespace orbi
{

std::optional<semver>
semver::parse(std::string_view str)
{
    using std::ranges::begin;
    using std::ranges::data;
    using std::ranges::end;
    using std::ranges::size;
    using std::views::split;

    auto versions{ str | split('.') };

    semver res;

    size_t i{ 0 };
    std::array arr{ std::ref(res.major_), std::ref(res.minor_), std::ref(res.patch_) };
    for (auto const v : versions)
    {
        if (i >= arr.size()) return std::nullopt;

        std::string_view const s{ begin(v), end(v) };
        if (s.size() > 1 && s.starts_with('0')) return std::nullopt;

        // NOLINTNEXTLINE
        if (auto const [_, ec] = std::from_chars(data(s), data(s) + size(s), arr[i].get());
            ec != std::errc{})
        {
            return std::nullopt;
        }

        i++;
    }

    if (i != arr.size()) return std::nullopt;

    return res;
}

semver::version
semver::major() const
{
    return major_;
}

semver::version
semver::minor() const
{
    return minor_;
}

semver::version
semver::patch() const
{
    return patch_;
}

std::string
semver::str() const
{
    return std::format("{}.{}.{}", major(), minor(), patch());
}

} // namespace orbi
