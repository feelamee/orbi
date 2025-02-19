#pragma once

#include <orbi/detail/pimpl.hpp>
#include <orbi/detail/util.hpp>
#include <orbi/exception.hpp>

#include <SDL3/SDL_vulkan.h>

namespace orbi
{

struct context;

struct window
{
public:
    // TODO unify error types of whole class to one enum
    //      encapsulate it inside exception class
    struct error : runtime_error
    {
        using runtime_error::runtime_error;
    };

    /*
        @pre `ctx` must outlive `*this`
        @throw `window::error`
    */
    explicit window(context const&);
    ~window();

    window(window&&) noexcept;
    window& operator=(window);

    friend void swap(window&, window&) noexcept;

    // TODO bitmasks already used in several classes,
    //      probably creating class `bitmask` make sense
    enum class flag
    {
        resizable = 0b1,
    };
    friend flag operator|(flag, flag);
    friend flag& operator|=(flag&, flag);
    friend flag operator&(flag, flag);

    /*
        @return bitmask of flags, failed to set

        use `bool(flag & window::flag::resizable)` to check on fail
        or `!bool(flag)` to check on success
    */
    flag set(flag) noexcept;

    struct impl;
    friend impl;

private:
    detail::pimpl<impl, 24, 8> data;
};

} // namespace orbi
