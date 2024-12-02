#pragma once

#include <orbi/exception.hpp>
#include <orbi/pimpl.hpp>

#include <any>

namespace orbi
{

struct ctx;

struct window
{
public:
    struct error : runtime_error
    {
        using runtime_error::runtime_error;
    };

    /*
        @pre `ctx` must outlive `*this`
        @throw `window::error`
    */
    window(ctx const&);
    ~window();

    window(window&&);
    window& operator=(window);

    window& operator=(window&&) = delete;

    window(window const&) = delete;
    window& operator=(window const&) = delete;

    friend void swap(window&, window&) noexcept;

    // TODO remove; created only to postpone design of api for creating surface
    std::any inner() noexcept;

    enum class flag
    {
        resizable = 0x1,
    };
    friend flag operator|(flag, flag);
    friend flag operator&(flag, flag);

    void set(flag);

private:
    struct impl;

    pimpl<impl, 8, 8> pimpl;
};

} // namespace orbi
