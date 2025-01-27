#pragma once

#include <orbi/ctx.hpp>
#include <orbi/device.hpp>
#include <orbi/window.hpp>

namespace orbi
{

struct ctx::impl
{
    static ctx::impl&
    from_ctx(ctx& c)
    {
        return *c.data;
    }

    static ctx::impl const&
    from_ctx(ctx const& c)
    {
        return *c.data;
    }

    vk::raii::Context vulkan_context;
    vk::raii::Instance vulkan_instance{ nullptr };
    vk::raii::DebugUtilsMessengerEXT debug_utils_messenger{ nullptr };
};

struct window::impl
{
    static window::impl&
    from_window(window& c)
    {
        return *c.data;
    }

    static window::impl const&
    from_window(window const& c)
    {
        return *c.data;
    }

    SDL_Window* sdl_window{ nullptr };
    ctx::impl const* context{ nullptr };
    VkSurfaceKHR vk_surface{};
};

struct device::impl
{
    static device::impl&
    from_device(device& c)
    {
        return *c.data;
    }

    static device::impl const&
    from_device(device const& c)
    {
        return *c.data;
    }

    vk::raii::PhysicalDevice vk_physical_device{ nullptr };
    vk::raii::Device vk_device{ nullptr };

    using queue_family_index_type = std::uint32_t;
    queue_family_index_type graphics_queue_family_index{ 0 };
    queue_family_index_type present_queue_family_index{ 0 };
};

} // namespace orbi
