#pragma once

#include <orbi/detail/pimpl.hpp>
#include <orbi/detail/util.hpp>
#include <orbi/exception.hpp>

#include <vulkan/vulkan_raii.hpp>

namespace orbi
{

struct version
{
    std::uint16_t major;
    std::uint16_t minor;
    std::uint16_t patch;
};

struct app_info
{
    std::string name{ "default application name" };
    version semver{ 0, 1, 0 };
};

struct ctx : detail::noncopyable
{
public:
    struct error : runtime_error
    {
        using runtime_error::runtime_error;
    };

    /*
        @throw `ctx::error`
    */
    ctx(app_info const& = {});
    ~ctx();

    ctx(ctx&&) noexcept;
    ctx& operator=(ctx);

    // TODO what about mixins? like `noncopyable`, `nonmoveable`, etc
    ctx& operator=(ctx&&) = delete;

    friend void swap(ctx&, ctx&) noexcept;

    struct impl;
    friend impl;

private:
    bool need_release_resource{ true };

    detail::pimpl<impl, 72, 8> data;
};

} // namespace orbi
