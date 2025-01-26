#pragma once

#include <orbi/detail/pimpl.hpp>
#include <orbi/detail/util.hpp>
#include <orbi/exception.hpp>
#include <orbi/version.hpp>

#include <vulkan/vulkan_raii.hpp>

#include <cstdint>

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

    enum struct subsystem : std::uint32_t
    {
        video = 0b01,
        event = 0b10,
    };
    friend subsystem operator|(subsystem, subsystem);
    friend subsystem operator&(subsystem, subsystem);
    friend subsystem operator|=(subsystem&, subsystem);
    friend subsystem operator&=(subsystem&, subsystem);

    /*
        init inner backend and required subsystems.
        Some of subsystems init can implie another.
        So, they will be inited too even if not required by caller

        @throw `ctx::error`
    */
    ctx(subsystem = {}, app_info const& = {});
    ~ctx();

    ctx(ctx&&);
    ctx& operator=(ctx);

    // TODO what about mixins? like `noncopyable`, `nonmoveable`, etc
    ctx& operator=(ctx&&) = delete;

    friend void swap(ctx&, ctx&) noexcept;

    subsystem inited_subsystems() const;

    // TODO remove; created only to postpone design of api
    // where is `std::optinal<T&>???`
    /*
        @return if `bool(subsystem::video & inited_subsystems())` `vk::raii::Instance`
                else                                              `nullptr`
    */
    vk::raii::Instance const* inner_vulkan_instance() const noexcept;

private:
    bool need_release_resource{ true };

    struct impl;
    detail::pimpl<impl, 80, 8> data;
};

} // namespace orbi
