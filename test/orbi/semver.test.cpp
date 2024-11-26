#include <doctest/doctest.h>

#include <orbi/semver.hpp>

#include <algorithm>
#include <random>
#include <ranges>
#include <string>

TEST_SUITE("orbi::version")
{

    // NOLINTNEXTLINE
    static std::mt19937 generator{ 42 };

    template <class... Args>
    struct always_true
    {
        bool
        operator()(Args...) const
        {
            return true;
        }
    };

    template <class Filter = always_true<std::string::value_type>>
    std::string::value_type random_char(Filter const& filter = {})
    {
        using value_type = std::string::value_type;
        using limits = std::numeric_limits<value_type>;

        using signed_integral = long long;
        using unsigned_integral = std::make_unsigned_t<signed_integral>;
        using integral = std::conditional_t<limits::is_signed, signed_integral, unsigned_integral>;

        static_assert(sizeof(integral) >= sizeof(value_type) &&
                      std::numeric_limits<integral>::is_signed == limits::is_signed);

        std::uniform_int_distribution<integral> dist{ limits::min(), limits::max() };

        value_type res{};

        do
        {
            res = static_cast<value_type>(dist(generator));
        } while (!filter(res));

        return res;
    }

    template <class Filter = always_true<std::string::value_type>>
    std::string random_string(std::string::size_type const size, Filter const& filter = {})
    {
        std::string res(size, '\0');

        std::ranges::generate(res, [&] { return random_char(filter); });

        return res;
    }

    template <std::integral T>
    T random_integral(T begin = std::numeric_limits<T>::min(), T end = std::numeric_limits<T>::max())
    {
        return std::uniform_int_distribution<T>{ begin, end }(generator);
    }

    TEST_CASE("satisfy alphabet")
    {
        auto const filter{ [&](auto const c) { return (c >= '0' && c <= '9') || c == '.'; } };

        for (auto const _ : std::views::iota(0, 100))
        {
            auto const s = random_string(random_integral(0, 100));
            if (orbi::semver::parse(s).has_value()) REQUIRE(std::ranges::any_of(s, filter));
        }
    }
}
