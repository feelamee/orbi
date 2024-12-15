#include <orbi/ctx.hpp>
#include <orbi/detail/util.hpp>
#include <orbi/device.hpp>
#include <orbi/window.hpp>

#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

#include <vulkan/vulkan_raii.hpp>

#include <utility>

namespace orbi
{

struct window::impl
{
    SDL_Window* window{ nullptr };
    ctx const* ctx{ nullptr };
    VkSurfaceKHR surface{};

    VkInstance
    vulkan_instance() const
    {
        assert(ctx->inited_subsystems() & ctx::subsystem::video);
        return **ctx->inner_vulkan_instance();
    }
};

window::window(ctx const& ctx)
{
    pimpl->window = SDL_CreateWindow("default window name", 500, 500, SDL_WINDOW_VULKAN);
    if (!pimpl->window)
    {
        throw error{ "SDL_CreateWindow failed with: '{}'", SDL_GetError() };
    }

    pimpl->ctx = &ctx;

    if (!SDL_Vulkan_CreateSurface(pimpl->window, pimpl->vulkan_instance(), nullptr, &pimpl->surface))
    {
        throw error{ "SDL_Vulkan_CreateSurface failed with: '{}'", SDL_GetError() };
    }
}

window::~window()
{
    // TODO: think about store ref instead of pointer to say more consistently
    //       that it can't be `nullptr`
    if (pimpl->window)
    {
        SDL_DestroyWindow(pimpl->window);
    }

    if (pimpl->surface)
    {
        SDL_Vulkan_DestroySurface(pimpl->vulkan_instance(), pimpl->surface, nullptr);
    }
}

window::window(window&& other)
{
    pimpl->window = std::exchange(other.pimpl->window, nullptr);
}

window&
window::operator=(window other)
{
    swap(*this, other);

    return *this;
}

void
swap(window& l, window& r) noexcept
{
    using std::swap;

    swap(l.pimpl, r.pimpl);
}

window::flag
operator|(window::flag const l, window::flag const r)
{
    return static_cast<window::flag>(detail::to_underlying(l) | detail::to_underlying(r));
}

window::flag&
operator|=(window::flag& l, window::flag const r)
{
    return l = l | r;
}

window::flag
operator&(window::flag l, window::flag r)
{
    return static_cast<window::flag>(detail::to_underlying(l) & detail::to_underlying(r));
}

window::set_error
window::set(flag const flags) noexcept
{
    set_error err{};

    if (bool(flags & flag::resizable))
    {
        if (!SDL_SetWindowResizable(pimpl->window, true))
        {
            err |= set_error::resizable;
        }
    }

    return err;
}

VkSurfaceKHR
window::inner_vulkan_surface() const
{
    return pimpl->surface;
}

} // namespace orbi
