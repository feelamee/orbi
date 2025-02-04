#include <orbi/context.hpp>
#include <orbi/detail/impl.hpp>
#include <orbi/detail/util.hpp>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_vulkan.h>

#include <vulkan/vulkan_raii.hpp>

#include <iostream>
#include <sstream>
#include <utility>

namespace orbi
{

namespace
{

VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT, unsigned int,
                                                 VkDebugUtilsMessengerCallbackDataEXT const*, void*);

}

context::context(app_info const& app_info)
try
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        throw error{ "SDL_Init failed with: '{}'", SDL_GetError() };
    }

    if (!SDL_Vulkan_LoadLibrary(nullptr))
    {
        throw error{ "SDL_Vulkan_LoadLibrary failed with: '{}'", SDL_GetError() };
    }

    vk::ApplicationInfo const vulkan_app_info{
        .pApplicationName = app_info.name.c_str(),
        .applicationVersion = vk::makeApiVersion(std::uint16_t{ 0 }, app_info.semver.major,
                                                 app_info.semver.minor, app_info.semver.patch),
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

    data->vulkan_instance = vk::raii::Instance{ data->vulkan_context, instance_create_info };

    data->debug_utils_messenger = [&]() -> vk::raii::DebugUtilsMessengerEXT
    {
        auto const severity_flags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

        auto const type_flags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                              vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                              vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

        return { data->vulkan_instance,
                 vk::DebugUtilsMessengerCreateInfoEXT{ .messageSeverity = severity_flags,
                                                       .messageType = type_flags,
                                                       .pfnUserCallback = &vk_debug_callback } };
    }();
}
catch (context::error const&)
{
    throw;
}
catch (...)
{
    std::throw_with_nested(error{ "ctx::ctx: Internal call to vulkan failed" });
}

context::~context()
{
    if (need_release_resource)
    {
        SDL_Quit();
        SDL_Vulkan_UnloadLibrary();
    }
}

context::context(context&& other) noexcept
    : need_release_resource(std::exchange(other.need_release_resource, false))
    , data(std::move(other.data))
{
}

context&
context::operator=(context other)
{
    swap(*this, other);

    return *this;
}

void
swap(context& l, context& r) noexcept
{
    using std::swap;

    swap(l.need_release_resource, r.need_release_resource);
}

namespace
{

VKAPI_ATTR VkBool32 VKAPI_CALL
vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, unsigned int type,
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
