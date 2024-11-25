#include <doctest/doctest.h>

#include <orbi/semver.hpp>

#include <format>

TEST_SUITE("orbi::version")
{
    TEST_CASE("parse major, minor, patch")
    {
        using version_type = orbi::semver::version;

        version_type major{ 0 };
        version_type minor{ 1 };
        version_type patch{ 2 };

        auto const version = orbi::semver::parse(std::format("{}.{}.{}", major, minor, patch));
        REQUIRE(version.has_value());

        REQUIRE_EQ(version->major(), major);
        REQUIRE_EQ(version->minor(), minor);
        REQUIRE_EQ(version->patch(), patch);
    }
}
