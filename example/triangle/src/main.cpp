#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

// workaround for bug https://github.com/libsdl-org/SDL/issues/11328
#undef VK_DEFINE_NON_DISPATCHABLE_HANDLE
#include <vulkan/vulkan_raii.hpp>

#include <cstdlib>
#include <format>
#include <functional>
#include <iostream>
#include <sstream>

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

constexpr auto const app_name = "probably triangle";
constexpr auto const engine_name = "orbi";

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

    if (0 < data->queueLabelCount)
    {
        message << "\tQueue Labels:\n";
        for (uint32_t i = 0; i < data->queueLabelCount; i++)
        {
            message << std::format("\t\tlabelName = <{}>\n", data->pQueueLabels[i].pLabelName);
        }
    }

    if (0 < data->cmdBufLabelCount)
    {
        message << "\tCommandBuffer Labels:\n";
        for (uint32_t i = 0; i < data->cmdBufLabelCount; i++)
        {
            message << std::format("\t\tlabelName = <{}>\n", data->pCmdBufLabels[i].pLabelName);
        }
    }

    if (0 < data->objectCount)
    {
        message << std::format("\tObjects:\n");
        for (uint32_t i = 0; i < data->objectCount; i++)
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

int
main()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        std::cout << std::format("SDL_Init failed with: '{}'", SDL_GetError()) << std::endl;
        return EXIT_FAILURE;
    }
    auto const destroy_sdl{ scope_exit([&]() { SDL_Quit(); }) };

    auto* const window{ SDL_CreateWindow(app_name, 720, 480, SDL_WINDOW_VULKAN) };
    if (!window)
    {
        std::cout << std::format("SDL_CreateWindow failed with: '{}'", SDL_GetError()) << std::endl;
        return EXIT_FAILURE;
    }
    auto const destroy_window{ scope_exit([&]() { SDL_DestroyWindow(window); }) };

    vk::raii::Context const ctx;
    vk::ApplicationInfo const application_info(app_name, vk::makeApiVersion(0, 0, 1, 0), engine_name,
                                               vk::makeApiVersion(0, 0, 1, 0), VK_API_VERSION_1_3);
    vk::InstanceCreateInfo instance_create_info({}, &application_info);

    auto const* const extensions_c_array =
        SDL_Vulkan_GetInstanceExtensions(&instance_create_info.enabledExtensionCount);
    if (!extensions_c_array)
    {
        std::cout << std::format("SDL_Vulkan_GetInstanceExtensions failed with: '{}'", SDL_GetError())
                  << std::endl;
        return EXIT_FAILURE;
    }

    std::vector<char const*> extensions{ extensions_c_array,
                                         // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                                         extensions_c_array + instance_create_info.enabledExtensionCount };

    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    instance_create_info.enabledExtensionCount = extensions.size();
    instance_create_info.ppEnabledExtensionNames = extensions.data();

#ifndef NDEBUG
    std::array<char const* const, 1> const layers{ "VK_LAYER_KHRONOS_validation" };
    instance_create_info.enabledLayerCount = layers.size();
    instance_create_info.ppEnabledLayerNames = layers.data();
#endif

    vk::raii::Instance const instance{ ctx, instance_create_info };

    auto const debug_utils_messenger{
        [&]() -> vk::raii::DebugUtilsMessengerEXT
        {
            using severity = vk::DebugUtilsMessageSeverityFlagBitsEXT;
            auto const severity_flags(severity::eWarning | severity::eError);

            using type = vk::DebugUtilsMessageTypeFlagBitsEXT;
            auto const type_flags(type::eGeneral | type::ePerformance | type::eValidation);

            vk::DebugUtilsMessengerCreateInfoEXT create_info({}, severity_flags, type_flags, &vk_debug_callback);

            return { instance, create_info };
        }()
    };

    vk::raii::PhysicalDevice const physical_device{ instance.enumeratePhysicalDevices().at(0) };

    std::uint32_t const queue_family_index{
        [&]
        {
            auto const queue_families{ physical_device.getQueueFamilyProperties() };
            auto const it = std::ranges::find_if(queue_families,
                                                 [](auto const props) {
                                                     return static_cast<bool>(props.queueFlags &
                                                                              vk::QueueFlagBits::eGraphics);
                                                 });
            assert(it != end(queue_families));

            return static_cast<std::uint32_t>(it - begin(queue_families));
        }()
    };

    float const queue_priority{ 1 };
    vk::DeviceQueueCreateInfo const queue_create_info{ {}, queue_family_index, 1, &queue_priority };
    vk::DeviceCreateInfo const device_create_info{ {},
                                                   1,
                                                   &queue_create_info,
#ifndef NDEBUG
                                                   layers.size(),
                                                   layers.data()
#endif
    };
    vk::raii::Device const device{ physical_device, device_create_info };

    auto* const renderer{ SDL_CreateRenderer(window, nullptr) };
    if (!renderer)
    {
        std::cout << std::format("SDL_CreateRenderer failed with: '{}'", SDL_GetError()) << std::endl;
        return EXIT_FAILURE;
    }
    auto const destroy_renderer{ scope_exit([&]() { SDL_DestroyRenderer(renderer); }) };

    if (!SDL_SetRenderDrawColor(renderer, 37, 5, 200, 255))
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

        if (!SDL_RenderClear(renderer))
        {
            std::cout << std::format("SDL_RenderClear failed with: '{}'", SDL_GetError()) << std::endl;
        }

        if (!SDL_RenderPresent(renderer))
        {
            std::cout << std::format("SDL_RenderPresent failed with: '{}'", SDL_GetError()) << std::endl;
        }
    }

    return EXIT_SUCCESS;
}
