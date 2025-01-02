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
    ctx const* context{ nullptr };
    VkSurfaceKHR surface{};

    VkInstance
    vulkan_instance() const
    {
        assert(context->inited_subsystems() & ctx::subsystem::video);
        return **context->inner_vulkan_instance();
    }
};

window::window(ctx const& ctx)
{
    data->window = SDL_CreateWindow("default window name", 500, 500, SDL_WINDOW_VULKAN);
    if (!data->window)
    {
        throw error{ "SDL_CreateWindow failed with: '{}'", SDL_GetError() };
    }

    data->context = &ctx;

    if (!SDL_Vulkan_CreateSurface(data->window, data->vulkan_instance(), nullptr, &data->surface))
    {
        throw error{ "SDL_Vulkan_CreateSurface failed with: '{}'", SDL_GetError() };
    }
}

window::~window()
{
    // TODO: think about store ref instead of pointer to say more consistently
    //       that it can't be `nullptr`
    if (data->window)
    {
        SDL_DestroyWindow(data->window);
    }

    if (data->surface)
    {
        SDL_Vulkan_DestroySurface(data->vulkan_instance(), data->surface, nullptr);
    }
}

window::window(window&& other)
{
    data->window = std::exchange(other.data->window, nullptr);
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

    swap(l.data, r.data);
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
        if (!SDL_SetWindowResizable(data->window, true))
        {
            err |= set_error::resizable;
        }
    }

    return err;
}

VkSurfaceKHR
window::inner_vulkan_surface() const
{
    return data->surface;
}

} // namespace orbi
