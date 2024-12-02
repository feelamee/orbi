#include <orbi/detail/util.hpp>
#include <orbi/window.hpp>

#include <SDL3/SDL_video.h>

#include <utility>

namespace orbi
{

struct window::impl
{
    SDL_Window* window{ nullptr };
};

window::window(ctx const& /*ctx*/)
{
    pimpl->window = SDL_CreateWindow("default window name", 500, 500, SDL_WINDOW_VULKAN);
    if (!pimpl->window)
    {
        throw error{ "SDL_CreateWindow failed with: '{}'", SDL_GetError() };
    }
}

window::~window()
{
    if (pimpl->window)
    {
        SDL_DestroyWindow(pimpl->window);
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

std::any
window::inner() noexcept
{
    return pimpl->window;
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

} // namespace orbi
