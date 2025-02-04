#include <doctest/doctest.h>

#include <orbi/context.hpp>
#include <orbi/detail/util.hpp>

TEST_SUITE("orbi")
{
    TEST_CASE("ctx::subsystem")
    {
        using namespace orbi;

        ctx::subsystem flags{};
        REQUIRE_FALSE(bool(flags));

        REQUIRE(bool(ctx::subsystem::video & ctx::subsystem::video));
        REQUIRE(bool(ctx::subsystem::event & ctx::subsystem::event));

        REQUIRE_FALSE(bool(ctx::subsystem::video & ctx::subsystem::event));
    }

    TEST_CASE("ctx::ctx")
    {
        try
        {
            orbi::ctx ctx;
        }
        catch (orbi::ctx::error const&)
        {
        }
        catch (...)
        {
            // `orbi::ctx::ctx` must throw only documented exception type
            REQUIRE(false);
        }
    }
}
