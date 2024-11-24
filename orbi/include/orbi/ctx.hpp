#pragma once

#include <orbi/exception.hpp>

#include <cstdint>

namespace orbi
{

struct ctx
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
        init inner backend and requred subsystems.
        Some of subsystems init can implie another.
        So, they will be inited too even if now required by caller

        @throw `ctx::error`
    */
    ctx(subsystem = {});
    ~ctx();

    ctx(ctx&&);
    ctx& operator=(ctx);

    // TODO what about mixins? like `noncopyable`, `nonmoveable`, etc
    ctx& operator=(ctx&&) = delete;

    ctx(ctx const&) = delete;
    ctx& operator=(ctx const&) = delete;

    friend void swap(ctx&, ctx&) noexcept;

private:
    bool need_release_resource{ true };
};

} // namespace orbi
