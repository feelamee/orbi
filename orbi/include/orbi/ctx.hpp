#pragma once

#include <orbi/detail/pimpl.hpp>
#include <orbi/detail/util.hpp>
#include <orbi/exception.hpp>
#include <orbi/version.hpp>

#include <vulkan/vulkan_raii.hpp>

namespace orbi
{

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

    // TODO remove; created only to postpone design of api
    // where is `std::optinal<T&>???`
    vk::raii::Instance const& inner_vulkan_instance() const noexcept;

private:
    bool need_release_resource{ true };

    struct impl;
    detail::pimpl<impl, 72, 8> data;
};

} // namespace orbi
