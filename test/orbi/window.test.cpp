#include <doctest/doctest.h>

#include <orbi/context.hpp>
#include <orbi/window.hpp>

TEST_SUITE("orbi")
{
    TEST_CASE("window::window")
    {
        orbi::ctx ctx;

        try
        {
            orbi::window window{ ctx };
        }
        catch (orbi::window::error const&)
        {
        }
        catch (...)
        {
            // `orbi::window::window` must throw only documented exception type
            REQUIRE(false);
        }
    }
}
