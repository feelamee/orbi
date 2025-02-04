#include <orbi/context.hpp>
#include <orbi/detail/impl.hpp>
#include <orbi/detail/util.hpp>
#include <orbi/device.hpp>
#include <orbi/window.hpp>

#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

#include <vulkan/vulkan_raii.hpp>

#include <utility>

namespace orbi
{

window::window(context const& ctx)
{
    data->sdl_window = SDL_CreateWindow("default window name", 500, 500, SDL_WINDOW_VULKAN);
    if (!data->sdl_window)
    {
        throw error{ "SDL_CreateWindow failed with: '{}'", SDL_GetError() };
    }

    data->ctx = &context::impl::from_ctx(ctx);

    if (!SDL_Vulkan_CreateSurface(data->sdl_window, *data->ctx->vulkan_instance, nullptr, &data->vk_surface))
    {
        throw error{ "SDL_Vulkan_CreateSurface failed with: '{}'", SDL_GetError() };
    }
}

window::~window()
{
    // TODO: think about store ref instead of pointer to say more consistently
    //       that it can't be `nullptr`
    if (data->sdl_window)
    {
        SDL_DestroyWindow(data->sdl_window);
    }

    if (data->vk_surface)
    {
        SDL_Vulkan_DestroySurface(*data->ctx->vulkan_instance, data->vk_surface, nullptr);
    }
}

window::window(window&& other) noexcept
{
    data->sdl_window = std::exchange(other.data->sdl_window, nullptr);
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

window::flag
window::set(flag const flags) noexcept
{
    flag err{};

    if (bool(flags & flag::resizable))
    {
        if (!SDL_SetWindowResizable(data->sdl_window, true))
        {
            err |= flag::resizable;
        }
    }

    return err;
}

} // namespace orbi
