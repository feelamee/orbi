#include <orbi/ctx.hpp>
#include <orbi/detail/util.hpp>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_vulkan.h>

#include <vulkan/vulkan_raii.hpp>

#include <iostream>
#include <optional>
#include <sstream>
#include <utility>

namespace orbi
{

namespace
{
VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
                                                 VkDebugUtilsMessengerCallbackDataEXT const*, void*);
}

struct ctx::impl
{
    struct video
    {
        video(app_info const& = {});

        vk::raii::Context vulkan_context;
        vk::raii::Instance vulkan_instance;
        vk::raii::DebugUtilsMessengerEXT debug_utils_messenger;
    };

    std::optional<video> video;
};

ctx::impl::video::video(app_info const& app_info)
try : vulkan_instance{ nullptr }, debug_utils_messenger{ nullptr }
{
    vk::ApplicationInfo const vulkan_app_info{
        .pApplicationName = app_info.name.c_str(),
        .applicationVersion = vk::makeApiVersion(std::uint16_t{ 0 }, app_info.version.major,
                                                 app_info.version.minor, app_info.version.patch),
        .pEngineName = "orbi",
        .engineVersion = vk::makeApiVersion(0, 0, 1, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    vk::InstanceCreateInfo instance_create_info{ .pApplicationInfo = &vulkan_app_info };

    auto const* const needed_extensions =
        SDL_Vulkan_GetInstanceExtensions(&instance_create_info.enabledExtensionCount);
    if (!needed_extensions)
    {
        throw error{ "SDL_Vulkan_GetInstanceExtensions failed with: '{}'", SDL_GetError() };
    }

    std::vector<char const*> extensions{ needed_extensions,
                                         // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                                         needed_extensions + instance_create_info.enabledExtensionCount };

    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    instance_create_info.enabledExtensionCount = extensions.size();
    instance_create_info.ppEnabledExtensionNames = extensions.data();

#ifndef NDEBUG
    std::array const layers{ "VK_LAYER_KHRONOS_validation" };

    instance_create_info.enabledLayerCount = layers.size();
    instance_create_info.ppEnabledLayerNames = layers.data();
#endif

    vulkan_instance = vk::raii::Instance{ vulkan_context, instance_create_info };

    debug_utils_messenger = {
        [&]() -> vk::raii::DebugUtilsMessengerEXT
        {
            auto const severity_flags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

            auto const type_flags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                  vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                  vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

            return { vulkan_instance,
                     { .messageSeverity = severity_flags, .messageType = type_flags, .pfnUserCallback = &vk_debug_callback } };
        }()
    };
}
catch (...)
{
    std::throw_with_nested(error{ "ctx::ctx: Internal call to vulkan failed" });
}

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

ctx::ctx(subsystem const flags, app_info const& app_info)
{
    SDL_InitFlags inner_flags{};

    if (bool(flags & subsystem::video)) inner_flags |= SDL_INIT_VIDEO;
    if (bool(flags & subsystem::event)) inner_flags |= SDL_INIT_EVENTS;

    if (!SDL_Init(inner_flags))
    {
        throw error{ "SDL_Init failed with: '{}'", SDL_GetError() };
    }

    if (bool(flags & subsystem::video))
    {
        if (!SDL_Vulkan_LoadLibrary(nullptr))
        {
            throw error{ "SDL_Vulkan_LoadLibrary failed with: '{}'", SDL_GetError() };
        }

        pimpl->video.emplace(app_info);
    }
}

ctx::~ctx()
{
    if (need_release_resource)
    {
        SDL_Quit();
    }

    if (pimpl->video.has_value())
    {
        SDL_Vulkan_UnloadLibrary();
    }
}

ctx::ctx(ctx&& other)
    : need_release_resource(std::exchange(other.need_release_resource, false))
{
}

ctx&
ctx::operator=(ctx other)
{
    swap(*this, other);

    return *this;
}

void
swap(ctx& l, ctx& r) noexcept
{
    using std::swap;

    swap(l.need_release_resource, r.need_release_resource);
}

std::any
ctx::inner_vulkan_context() noexcept
{
    std::any res;

    if (pimpl->video.has_value())
    {
        res = std::ref(pimpl->video->vulkan_context);
    }

    return res;
}

std::any
ctx::inner_vulkan_instance() const noexcept
{
    std::any res;

    if (pimpl->video.has_value())
    {
        res = std::ref(pimpl->video->vulkan_instance);
    }

    return res;
}

namespace
{
VKAPI_ATTR VkBool32 VKAPI_CALL
vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
                  VkDebugUtilsMessengerCallbackDataEXT const* data, void* /*user_data*/)
{
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    std::ostringstream message;

    // clang-format off
        message << std::format("{}: {}:\n"
                               "\tmessageIdName = <{}>\n"
                               "\tmessageIdNumber = <{:#x}>\n"
                               "\tmessage = <{}>\n",
                               vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(severity)),
                               vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(type)),
                               data->pMessageIdName,
                               data->messageIdNumber,
                               data->pMessage);
    // clang-format on

    if (data->queueLabelCount > 0)
    {
        message << "\tQueue Labels:\n";
        for (uint32_t i{ 0 }; i < data->queueLabelCount; i++)
        {
            message << std::format("\t\tlabelName = <{}>\n", data->pQueueLabels[i].pLabelName);
        }
    }

    if (data->cmdBufLabelCount > 0)
    {
        message << "\tCommandBuffer Labels:\n";
        for (uint32_t i = 0; i < data->cmdBufLabelCount; i++)
        {
            message << std::format("\t\tlabelName = <{}>\n", data->pCmdBufLabels[i].pLabelName);
        }
    }

    if (data->objectCount > 0)
    {
        message << std::format("\tObjects:\n");
        for (uint32_t i{ 0 }; i < data->objectCount; i++)
        {
            // clang-format off
                message << std::format("\t\tObject {}\n"
                                       "\t\t\tobjectType = {}\n"
                                       "\t\t\tobjectHandle = {:#x}\n",
                                       i,
                                       vk::to_string(static_cast<vk::ObjectType>(data->pObjects[i].objectType)),
                                       data->pObjects[i].objectHandle);

                if (data->pObjects[i].pObjectName)
                {
                    message << std::format("\t\t\tobjectName = <{}>\n", data->pObjects[i].pObjectName);
                }
            // clang-format on
        }
    }

    std::cout << message.str() << std::endl;

    return false;
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}
} // namespace

} // namespace orbi
