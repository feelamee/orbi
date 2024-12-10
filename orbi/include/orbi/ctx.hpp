#pragma once

#include <orbi/detail/util.hpp>
#include <orbi/exception.hpp>
#include <orbi/pimpl.hpp>
#include <orbi/version.hpp>

#include <any>
#include <cstdint>

namespace orbi
{

struct app_info
{
    std::string name{ "default application name" };
    version version{ 0, 1, 0 };
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

    // TODO remove; created only to postpone design of api
    /*
        @return `std::reference_wrapper<vk::raii::Context>`
                if `ctx` was initialized with `subsystem::video`
                Else `ret.has_value() == false`, where `ret` is returned object
    */
    std::any inner_vulkan_context() noexcept;
    /*
        @return `std::reference_wrapper<vk::raii::Instance const>`
                if `ctx` was initialized with `subsystem::video`.
                Else `ret.has_value() == false`, where `ret` is returned object
    */
    std::any inner_vulkan_instance() const noexcept;

private:
    bool need_release_resource{ true };

    struct impl;
    pimpl<impl, 80, 8> pimpl;
};

} // namespace orbi
