#include <doctest/doctest.h>

#include <orbi/context.hpp>
#include <orbi/detail/util.hpp>

TEST_SUITE("orbi")
{
    TEST_CASE("context::context")
    {
        try
        {
            orbi::context ctx;
        }
        catch (orbi::context::error const&)
        {
        }
        catch (...)
        {
            // `context::context` must throw only documented exception type
            REQUIRE(false);
        }
    }
}
