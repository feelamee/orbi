#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

// workaround for bug https://github.com/libsdl-org/SDL/issues/11328
#undef VK_DEFINE_NON_DISPATCHABLE_HANDLE
#include <vulkan/vulkan_raii.hpp>

#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <ranges>
#include <sstream>
#include <stdexcept>

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

struct sdl_error : std::runtime_error
{

    explicit sdl_error(std::string const& msg)
        : runtime_error{ msg }
    {
    }

    sdl_error(char const* msg)
        : runtime_error{ msg }
    {
    }
};

std::vector<std::byte>
read_file(std::filesystem::path const& filename)
{
    std::ifstream file(filename, std::ios::binary);
    file.exceptions(std::ifstream::badbit);

    auto const size = std::filesystem::file_size(filename);
    std::vector<std::byte> buffer{ size };

    file.read(reinterpret_cast<std::ifstream::char_type*>(buffer.data()), static_cast<std::streamsize>(size));

    return buffer;
}

int
main()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        throw sdl_error{ std::format("SDL_Init failed with: '{}'", SDL_GetError()) };
    }
    auto const destroy_sdl{ scope_exit([&]() { SDL_Quit(); }) };

    auto* const window{ SDL_CreateWindow(app_name, 720, 480, SDL_WINDOW_VULKAN) };
    if (!window)
    {
        throw sdl_error{ std::format("SDL_CreateWindow failed with: '{}'", SDL_GetError()) };
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
        throw sdl_error{ std::format("SDL_Vulkan_GetInstanceExtensions failed with: '{}'", SDL_GetError()) };
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

    auto const surface{ [&]() -> vk::raii::SurfaceKHR
                        {
                            VkSurfaceKHR raw_surface{};
                            if (!SDL_Vulkan_CreateSurface(window, *instance, nullptr, &raw_surface))
                            {
                                throw sdl_error{
                                    std::format("SDL_Vulkan_CreateSurface failed with: '{}'", SDL_GetError())
                                };
                            }

                            return { instance, raw_surface };
                        }() };

    vk::raii::PhysicalDevice const physical_device{ instance.enumeratePhysicalDevices().at(0) };

    using queue_family_index_type = std::uint32_t;

    auto const graphics_queue_family_index{
        [&]() -> queue_family_index_type
        {
            auto const queue_families{ physical_device.getQueueFamilyProperties() };
            auto const it{ std::ranges::find_if(queue_families,
                                                [](auto const props) {
                                                    return static_cast<bool>(props.queueFlags &
                                                                             vk::QueueFlagBits::eGraphics);
                                                }) };

            assert(it != end(queue_families));

            return it - begin(queue_families);
        }()
    };

    auto const present_queue_family_index{
        [&]() -> queue_family_index_type
        {
            auto const queue_families{ physical_device.getQueueFamilyProperties() };
            auto const it{
                std::ranges::find_if(queue_families, [&, i = 0](auto const) mutable
                                     { return physical_device.getSurfaceSupportKHR(i++, *surface); })
            };

            assert(it != end(queue_families));

            return it - begin(queue_families);
        }()
    };

    std::vector unique_queue_families{ graphics_queue_family_index, present_queue_family_index };
    std::ranges::sort(unique_queue_families);
    auto const [first, last] = std::ranges::unique(unique_queue_families);
    unique_queue_families.erase(first, last);

    float const queue_priority{ 1 };
    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    for (auto const& qf : unique_queue_families)
    {
        queue_create_infos.emplace_back(vk::DeviceQueueCreateFlags{}, qf, 1, &queue_priority);
    }

    std::array<char const* const, 1> const device_extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    vk::DeviceCreateInfo const device_create_info{ {},
                                                   static_cast<std::uint32_t>(queue_create_infos.size()),
                                                   queue_create_infos.data(),
#ifndef NDEBUG
                                                   layers.size(),
                                                   layers.data(),
#endif
                                                   device_extensions.size(),
                                                   device_extensions.data()

    };
    vk::raii::Device device{ physical_device, device_create_info };

    vk::raii::Queue const graphics_queue{ device.getQueue(graphics_queue_family_index, 0) };
    vk::raii::Queue const present_queue{ device.getQueue(present_queue_family_index, 0) };

    auto const surface_capabilities{ physical_device.getSurfaceCapabilitiesKHR(*surface) };
    std::vector const surface_formats{ physical_device.getSurfaceFormatsKHR(*surface) };
    std::vector const surface_present_modes{ physical_device.getSurfacePresentModesKHR(*surface) };

    assert(!surface_formats.empty() && !surface_present_modes.empty());

    auto const surface_format{ std::ranges::find(surface_formats,
                                                 vk::SurfaceFormatKHR{ vk::Format::eB8G8R8A8Srgb,
                                                                       vk::ColorSpaceKHR::eSrgbNonlinear }) };
    assert(surface_format != end(surface_formats));

    auto const surface_present_mode{ std::ranges::find(surface_present_modes, vk::PresentModeKHR::eFifo) };
    assert(surface_present_mode != end(surface_present_modes));

    vk::Extent2D const extent{ surface_capabilities.currentExtent };

    vk::raii::SwapchainKHR const swapchain{
        device,
        vk::SwapchainCreateInfoKHR{
            {},
            *surface,
            std::clamp(3u, surface_capabilities.minImageCount,
                       surface_capabilities.maxImageCount == 0 ? std::numeric_limits<std::uint32_t>::max()
                                                               : surface_capabilities.maxImageCount),
            surface_format->format,
            surface_format->colorSpace,
            extent,
            1,
            vk::ImageUsageFlagBits::eColorAttachment,
            unique_queue_families.size() == 1 ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent,
            static_cast<std::uint32_t>(unique_queue_families.size()),
            unique_queue_families.data(),
            surface_capabilities.currentTransform,
            vk::CompositeAlphaFlagBitsKHR::eOpaque,
            *surface_present_mode,
            vk::True }
    };

    std::vector const images{ swapchain.getImages() };

    auto const image_views_range{
        std::views::transform(images,
                              [&](auto const& image)
                              {
                                  return device.createImageView(vk::ImageViewCreateInfo{
                                      {},
                                      image,
                                      vk::ImageViewType::e2D,
                                      surface_format->format,
                                      vk::ComponentMapping{},
                                      vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } });
                              })
    };
    std::vector const image_views(begin(image_views_range), end(image_views_range));

    auto const res_dir{ std::filesystem::current_path() / "example/triangle/res" };
    auto const vertex_shader_bytecode{ read_file(res_dir / "triangle.vert.spv") };
    auto const fragment_shader_bytecode{ read_file(res_dir / "triangle.frag.spv") };

    vk::raii::ShaderModule const vertex_shader{
        device,
        vk::ShaderModuleCreateInfo{ {},
                                    vertex_shader_bytecode.size(),
                                    reinterpret_cast<std::uint32_t const*>(vertex_shader_bytecode.data()) }
    };

    vk::raii::ShaderModule const fragment_shader{
        device,
        vk::ShaderModuleCreateInfo{ {},
                                    fragment_shader_bytecode.size(),
                                    reinterpret_cast<std::uint32_t const*>(fragment_shader_bytecode.data()) }
    };

    std::array const shader_stage_create_infos{
        vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eVertex, vertex_shader, "main" },
        vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eFragment, fragment_shader, "main" },
    };

    vk::PipelineVertexInputStateCreateInfo const vertex_input_state_create_info{};
    vk::PipelineInputAssemblyStateCreateInfo const input_assembly_state_create_info{ {}, vk::PrimitiveTopology::eTriangleList };

    vk::Viewport const viewport{
        0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0, 1
    };

    vk::Rect2D const scissor{ { 0, 0 }, extent };

    vk::PipelineViewportStateCreateInfo const viewport_state_create_info{ {}, 1, &viewport, 1, &scissor };

    vk::PipelineRasterizationStateCreateInfo const rasterization_state_create_info{ {},
                                                                                    vk::False,
                                                                                    vk::False,
                                                                                    vk::PolygonMode::eFill,
                                                                                    vk::CullModeFlagBits::eBack,
                                                                                    vk::FrontFace::eClockwise,
                                                                                    vk::False,
                                                                                    0,
                                                                                    0,
                                                                                    0,
                                                                                    1 };

    vk::PipelineColorBlendAttachmentState const color_blend_attachment_state{
        vk::True,          vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusDstAlpha,
        vk::BlendOp::eAdd, vk::BlendFactor::eOne,      vk::BlendFactor::eZero,
        vk::BlendOp::eAdd
    };

    vk::PipelineColorBlendStateCreateInfo const color_blend_state_create_info{ {},
                                                                               vk::False,
                                                                               vk::LogicOp::eCopy,
                                                                               1,
                                                                               &color_blend_attachment_state };

    vk::raii::PipelineLayout const layout{ device, vk::PipelineLayoutCreateInfo{} };

    vk::AttachmentDescription const attachment_description{ {},
                                                            surface_format->format,
                                                            vk::SampleCountFlagBits::e1,
                                                            vk::AttachmentLoadOp::eClear,
                                                            vk::AttachmentStoreOp::eStore,
                                                            {},
                                                            {},
                                                            vk::ImageLayout::eUndefined,
                                                            vk::ImageLayout::ePresentSrcKHR };

    vk::AttachmentReference const attachment_reference{ 0, vk::ImageLayout::eColorAttachmentOptimal };

    vk::SubpassDescription const subpass_description{ {}, vk::PipelineBindPoint::eGraphics,
                                                      {}, {},
                                                      1,  &attachment_reference };

    vk::SubpassDependency const subpass_dependency{ vk::SubpassExternal,
                                                    0,
                                                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                                    vk::AccessFlagBits::eNone,
                                                    vk::AccessFlagBits::eColorAttachmentWrite };

    vk::raii::RenderPass const render_pass{
        device, vk::RenderPassCreateInfo{ {}, 1, &attachment_description, 1, &subpass_description, 1, &subpass_dependency }
    };

    vk::PipelineMultisampleStateCreateInfo const multisample_state_create_info{ {},
                                                                                vk::SampleCountFlagBits::e1,
                                                                                vk::False,
                                                                                1 };

    std::array const dynamic_states{ vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo const dynamic_state_create_info{ {},
                                                                        dynamic_states.size(),
                                                                        dynamic_states.data() };

    vk::raii::Pipeline const pipeline{ device, nullptr,
                                       vk::GraphicsPipelineCreateInfo{ {},
                                                                       shader_stage_create_infos.size(),
                                                                       shader_stage_create_infos.data(),
                                                                       &vertex_input_state_create_info,
                                                                       &input_assembly_state_create_info,
                                                                       {},
                                                                       &viewport_state_create_info,
                                                                       &rasterization_state_create_info,
                                                                       &multisample_state_create_info,
                                                                       {},
                                                                       &color_blend_state_create_info,
                                                                       &dynamic_state_create_info,
                                                                       *layout,
                                                                       *render_pass,
                                                                       0 } };

    auto const frame_buffers_range{ std::views::transform(
        image_views,
        [&](auto const& view)
        {
            return vk::raii::Framebuffer{
                device, vk::FramebufferCreateInfo{ {}, render_pass, 1, &*view, extent.width, extent.height, 1 }
            };
        }) };
    std::vector const frame_buffers(begin(frame_buffers_range), end(frame_buffers_range));

    vk::raii::CommandPool const command_pool{
        device, vk::CommandPoolCreateInfo{ vk::CommandPoolCreateFlagBits::eResetCommandBuffer, graphics_queue_family_index }
    };

    std::vector const command_buffers{ device.allocateCommandBuffers(
        vk::CommandBufferAllocateInfo{ command_pool, vk::CommandBufferLevel::ePrimary, 1 }) };
    auto const& command_buffer{ command_buffers.at(0) };

    vk::raii::Semaphore image_available_semaphore{ device, vk::SemaphoreCreateInfo{} };
    vk::raii::Semaphore render_finish_semaphore{ device, vk::SemaphoreCreateInfo{} };
    vk::raii::Fence fence{ device, vk::FenceCreateInfo{ vk::FenceCreateFlagBits::eSignaled } };

    while (true)
    {
        auto constexpr timeout{ 3'000'000'000 /* std::numeric_limits<std::uint64_t>::max() */ };
        assert(vk::Result::eSuccess == device.waitForFences(*fence, vk::True, timeout));
        device.resetFences(*fence);

        SDL_Event ev{};
        while (SDL_PollEvent(&ev))
        {
            switch (ev.type)
            {
            case SDL_EVENT_QUIT:
                return EXIT_SUCCESS;
            }
        }

        auto const [result, image_index]{ swapchain.acquireNextImage(timeout, image_available_semaphore) };
        assert(vk::Result::eSuccess == result);

        command_buffer.reset();

        command_buffer.begin(vk::CommandBufferBeginInfo{});

        vk::ClearValue const color{ { 37, 5, 200, 255 } };

        command_buffer.beginRenderPass(vk::RenderPassBeginInfo{ render_pass, frame_buffers[image_index],
                                                                vk::Rect2D{ { 0, 0 }, extent }, 1, &color },
                                       vk::SubpassContents::eInline);

        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        command_buffer.setViewport(0, viewport);
        command_buffer.setScissor(0, scissor);
        command_buffer.draw(3, 1, 0, 0);

        command_buffer.endRenderPass();
        command_buffer.end();

        vk::PipelineStageFlags const stage_flag{ vk::PipelineStageFlagBits::eColorAttachmentOutput };
        graphics_queue.submit(vk::SubmitInfo{ 1, &*image_available_semaphore, &stage_flag, 1,
                                              &*command_buffer, 1, &*render_finish_semaphore },
                              fence);

        assert(vk::Result::eSuccess == present_queue.presentKHR(vk::PresentInfoKHR{
                                           1, &*render_finish_semaphore, 1, &*swapchain, &image_index }));
    }

    return EXIT_SUCCESS;
}
