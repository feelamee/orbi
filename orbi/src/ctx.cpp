#include <orbi/ctx.hpp>
#include <orbi/util.hpp>

#include <SDL3/SDL_init.h>

namespace orbi
{

ctx::subsystem
operator|(ctx::subsystem l, ctx::subsystem r)
{
    return static_cast<ctx::subsystem>(detail::to_underlying(l) | detail::to_underlying(r));
}

ctx::subsystem
operator&(ctx::subsystem l, ctx::subsystem r)
{
    return static_cast<ctx::subsystem>(detail::to_underlying(l) & detail::to_underlying(r));
}

ctx::ctx(subsystem const flags)
{
    SDL_InitFlags inner_flags{};

    if (bool(flags & subsystem::video)) inner_flags |= SDL_INIT_VIDEO;
    if (bool(flags & subsystem::event)) inner_flags |= SDL_INIT_EVENTS;

    if (!SDL_Init(inner_flags))
    {
        throw error{ "SDL_Init failed with: '{}'", SDL_GetError() };
    }
}

ctx::~ctx()
{
    SDL_Quit();
}

} // namespace orbi
