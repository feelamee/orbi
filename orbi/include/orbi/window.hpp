#pragma once

#include <orbi/detail/util.hpp>
#include <orbi/exception.hpp>
#include <orbi/pimpl.hpp>

#include <any>

namespace orbi
{

struct ctx;

struct window : detail::noncopyable
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
    explicit window(ctx const&);
    ~window();

    window(window&&);
    window& operator=(window);

    window& operator=(window&&) = delete;

    friend void swap(window&, window&) noexcept;

    // TODO remove; created only to postpone design of api for creating surface
    std::any inner() noexcept;

    // TODO bitmasks already used in several classes,
    //      probably creating class `bitmask` make sense
    enum class flag
    {
        resizable = 0x1,
    };
    friend flag operator|(flag, flag);
    friend flag& operator|=(flag&, flag);
    friend flag operator&(flag, flag);

    /*
        @return bitmask of flags, failed to set

        use `bool(flag & window::flag::resizable)` to check on fail
        or `!bool(flag)` to check on success
    */
    using set_error = flag;

    set_error set(flag) noexcept;

    // TODO remove; created only to postpone design of api
    /*
        @return `VkSurface`
    */
    std::any inner_vulkan_surface() const;

private:
    struct impl;

    pimpl<impl, 24, 8> pimpl;
};

} // namespace orbi
