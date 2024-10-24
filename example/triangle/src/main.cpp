#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>

#include <cstdlib>
#include <format>
#include <functional>
#include <iostream>

template <typename Fn>
struct scope_exit
{
public:
    scope_exit(scope_exit const&) = delete;
    scope_exit(scope_exit&&) = delete;
    scope_exit& operator=(scope_exit const&) = delete;
    scope_exit& operator=(scope_exit&&) = delete;

    scope_exit(Fn&& f)
        : fn{ f }
    {
    }

    ~scope_exit()
    {
        std::invoke(std::move(fn));
    }

private:
    Fn fn;
};

int
main()
{
    if (0 != SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        std::cout << std::format("SDL_Init failed with: '{}'", SDL_GetError()) << std::endl;
        return EXIT_FAILURE;
    }
    auto const destroy_sdl{ scope_exit([&]() { SDL_Quit(); }) };

    auto* const window{ SDL_CreateWindow("probably triangle", 720, 480, SDL_WINDOW_VULKAN) };
    if (!window)
    {
        std::cout << std::format("SDL_CreateWindow failed with: '{}'", SDL_GetError()) << std::endl;
        return EXIT_FAILURE;
    }
    auto const destroy_window{ scope_exit([&]() { SDL_DestroyWindow(window); }) };

    auto* const renderer{ SDL_CreateRenderer(window, nullptr) };
    if (!renderer)
    {
        std::cout << std::format("SDL_CreateRenderer failed with: '{}'", SDL_GetError()) << std::endl;
        return EXIT_FAILURE;
    }
    auto const destroy_renderer{ scope_exit([&]() { SDL_DestroyRenderer(renderer); }) };

    if (0 != SDL_SetRenderDrawColor(renderer, 37, 5, 200, 255))
    {
        std::cout << std::format("SDL_SetRenderDrawColor failed with: '{}'", SDL_GetError()) << std::endl;
        return EXIT_FAILURE;
    }

    bool running{ true };
    while (running)
    {
        SDL_Event ev{};
        while (SDL_PollEvent(&ev))
        {
            switch (ev.type)
            {
            case SDL_EVENT_QUIT:
                running = false;
            }
        }

        if (0 != SDL_RenderClear(renderer))
        {
            std::cout << std::format("SDL_RenderClear failed with: '{}'", SDL_GetError()) << std::endl;
        }

        if (0 != SDL_RenderPresent(renderer))
        {
            std::cout << std::format("SDL_RenderPresent failed with: '{}'", SDL_GetError()) << std::endl;
        }
    }

    return EXIT_SUCCESS;
}
