#include <doctest/doctest.h>

#include <orbi/semver.hpp>

#include <algorithm>
#include <ranges>

TEST_SUITE("orbi::semver")
{
    struct fold_left_fn
    {
        template <std::input_iterator I, std::sentinel_for<I> S, class T = std::iter_value_t<I>,
                  class F>
        constexpr auto
        operator()(I first, S last, T init, F f) const
        {
            using result = std::decay_t<std::invoke_result_t<F&, T, std::iter_reference_t<I>>>;

            if (first == last) return result(std::move(init));

            result accum = std::invoke(f, std::move(init), *first);

            for (++first; first != last; ++first) accum = std::invoke(f, std::move(accum), *first);

            return std::move(accum);
        }

        template <std::ranges::input_range R, class T = std::ranges::range_value_t<R>, class F>
        constexpr auto
        operator()(R&& r, T init, F f) const
        {
            return (*this)(std::ranges::begin(r), std::ranges::end(r), std::move(init),
                           std::ref(f));
        }
    };

    inline constexpr fold_left_fn fold_left;

    TEST_CASE("major minor patch str")
    {
        using namespace std::string_view_literals;
        using namespace orbi;

        std::array const strings = {
            ""sv, "a.b.c"sv, "1.2.3.4"sv, "1..234"sv, "01.002.003"sv, "0.0.0"sv,
        };

        auto const filter{ [&](auto const c) { return (c >= '0' && c <= '9') || c == '.'; } };

        for (auto const s : strings)
        {
            CAPTURE(s);

            auto const maybe_semver = semver::parse(s);

            if (std::ranges::any_of(s, std::not_fn(filter)))
            {
                REQUIRE_FALSE(maybe_semver.has_value());
                continue;
            }

            static int const min_size{ 5 };

            if (std::ranges::count(s, '.') != 2 || s.size() < min_size)
            {
                REQUIRE_FALSE(maybe_semver.has_value());
                continue;
            }

            if (std::ranges::adjacent_find(s, [&](auto const l, auto const r)
                                           { return l == '.' && r == '.'; })
                != end(s))
            {
                REQUIRE_FALSE(maybe_semver.has_value());
                continue;
            }

            if (std::ranges::count(s, '.') != 2 || s.size() < 5)
            {
                REQUIRE_FALSE(maybe_semver.has_value());
                continue;
            }

            using std::ranges::size;
            using std::views::split;

            if (fold_left(s | split('.'), false, [](bool is_invalid, auto&& s)
                          { return is_invalid || (size(s) > 1 ? s[0] == '0' : false); }))
            {
                REQUIRE_FALSE(maybe_semver.has_value());
                continue;
            }

            REQUIRE(maybe_semver.has_value());
        }

        for (auto const _ : std::views::iota(0, 100))
        {
            using version = semver::version;
            version major{ static_cast<version>(rand()) };
            version minor{ static_cast<version>(rand()) };
            version patch{ static_cast<version>(rand()) };

            auto const s = std::format("{}.{}.{}", major, minor, patch);
            CAPTURE(s);

            auto const ver{ semver::parse(s) };
            REQUIRE(ver.has_value());
            REQUIRE_EQ(ver->major(), major);
            REQUIRE_EQ(ver->minor(), minor);
            REQUIRE_EQ(ver->patch(), patch);
            REQUIRE_EQ(s, ver->str());
        }
    }
}
