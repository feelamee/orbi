#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

// workaround for bug https://github.com/libsdl-org/SDL/issues/11328
#undef VK_DEFINE_NON_DISPATCHABLE_HANDLE
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_SPACESHIP_OPERATOR
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
    vk::ApplicationInfo const application_info{ .pApplicationName = app_name,
                                                .applicationVersion = vk::makeApiVersion(0, 0, 1, 0),
                                                .pEngineName = engine_name,
                                                .engineVersion = vk::makeApiVersion(0, 0, 1, 0),
                                                .apiVersion = VK_API_VERSION_1_3 };
    vk::InstanceCreateInfo instance_create_info{ .pApplicationInfo = &application_info };

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

            vk::DebugUtilsMessengerCreateInfoEXT create_info{ .messageSeverity = severity_flags,
                                                              .messageType = type_flags,
                                                              .pfnUserCallback = &vk_debug_callback };

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
        queue_create_infos.push_back({ .queueFamilyIndex = qf, .queueCount = 1, .pQueuePriorities = &queue_priority });
    }

    std::array<char const* const, 1> const device_extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    vk::DeviceCreateInfo const device_create_info{
        .queueCreateInfoCount = static_cast<std::uint32_t>(queue_create_infos.size()),
        .pQueueCreateInfos = queue_create_infos.data(),
#ifndef NDEBUG
        .enabledLayerCount = layers.size(),
        .ppEnabledLayerNames = layers.data(),
#endif
        .enabledExtensionCount = device_extensions.size(),
        .ppEnabledExtensionNames = device_extensions.data()

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
        { .surface = *surface,
          .minImageCount = std::clamp(3u, surface_capabilities.minImageCount,
                                      surface_capabilities.maxImageCount == 0
                                          ? std::numeric_limits<std::uint32_t>::max()
                                          : surface_capabilities.maxImageCount),
          .imageFormat = surface_format->format,
          .imageColorSpace = surface_format->colorSpace,
          .imageExtent = extent,
          .imageArrayLayers = 1,
          .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
          .imageSharingMode = unique_queue_families.size() == 1 ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent,
          .queueFamilyIndexCount = static_cast<std::uint32_t>(unique_queue_families.size()),
          .pQueueFamilyIndices = unique_queue_families.data(),
          .preTransform = surface_capabilities.currentTransform,
          .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
          .presentMode = *surface_present_mode,
          .clipped = vk::True }
    };

    std::vector const images{ swapchain.getImages() };

    auto const image_views_range{
        std::views::transform(images,
                              [&](auto const& image)
                              {
                                  return device.createImageView(
                                      { .image = image,
                                        .viewType = vk::ImageViewType::e2D,
                                        .format = surface_format->format,
                                        .subresourceRange = { .aspectMask = vk::ImageAspectFlagBits::eColor,
                                                              .baseMipLevel = 0,
                                                              .levelCount = 1,
                                                              .baseArrayLayer = 0,
                                                              .layerCount = 1 } });
                              })
    };
    std::vector const image_views(begin(image_views_range), end(image_views_range));

    auto const res_dir{ std::filesystem::current_path() / "example/triangle/res" };
    auto const vertex_shader_bytecode{ read_file(res_dir / "triangle.vert.spv") };
    auto const fragment_shader_bytecode{ read_file(res_dir / "triangle.frag.spv") };

    vk::raii::ShaderModule const vertex_shader{ device,
                                                { .codeSize = vertex_shader_bytecode.size(),
                                                  .pCode = reinterpret_cast<std::uint32_t const*>(
                                                      vertex_shader_bytecode.data()) } };

    vk::raii::ShaderModule const fragment_shader{ device,
                                                  { .codeSize = fragment_shader_bytecode.size(),
                                                    .pCode = reinterpret_cast<std::uint32_t const*>(
                                                        fragment_shader_bytecode.data()) } };

    std::array const shader_stage_create_infos{
        vk::PipelineShaderStageCreateInfo{ .stage = vk::ShaderStageFlagBits::eVertex,
                                           .module = vertex_shader,
                                           .pName = "main" },
        vk::PipelineShaderStageCreateInfo{ .stage = vk::ShaderStageFlagBits::eFragment,
                                           .module = fragment_shader,
                                           .pName = "main" },
    };

    vk::PipelineVertexInputStateCreateInfo const vertex_input_state_create_info{};
    vk::PipelineInputAssemblyStateCreateInfo const input_assembly_state_create_info{
        .topology = vk::PrimitiveTopology::eTriangleList
    };

    vk::Viewport const viewport{ .x = 0,
                                 .y = 0,
                                 .width = static_cast<float>(extent.width),
                                 .height = static_cast<float>(extent.height),
                                 .minDepth = 0,
                                 .maxDepth = 1 };

    vk::Rect2D const scissor{ .offset = { 0, 0 }, .extent = extent };

    vk::PipelineViewportStateCreateInfo const viewport_state_create_info{ .viewportCount = 1,
                                                                          .pViewports = &viewport,
                                                                          .scissorCount = 1,
                                                                          .pScissors = &scissor };

    vk::PipelineRasterizationStateCreateInfo const rasterization_state_create_info{
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eBack,
        .frontFace = vk::FrontFace::eClockwise,
        .depthBiasEnable = vk::False,
        .depthBiasConstantFactor = 0,
        .depthBiasClamp = 0,
        .depthBiasSlopeFactor = 0,
        .lineWidth = 1
    };

    vk::PipelineColorBlendAttachmentState const color_blend_attachment_state{
        .blendEnable = vk::False,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    };

    vk::PipelineColorBlendStateCreateInfo const color_blend_state_create_info{ .logicOpEnable = vk::False,
                                                                               .logicOp = vk::LogicOp::eCopy,
                                                                               .attachmentCount = 1,
                                                                               .pAttachments = &color_blend_attachment_state };

    vk::raii::PipelineLayout const layout{ device, vk::PipelineLayoutCreateInfo{} };

    vk::AttachmentDescription const attachment_description{ .format = surface_format->format,
                                                            .samples = vk::SampleCountFlagBits::e1,
                                                            .loadOp = vk::AttachmentLoadOp::eClear,
                                                            .storeOp = vk::AttachmentStoreOp::eStore,
                                                            .initialLayout = vk::ImageLayout::eUndefined,
                                                            .finalLayout = vk::ImageLayout::ePresentSrcKHR };

    vk::AttachmentReference const attachment_reference{ .attachment = 0, .layout = vk::ImageLayout::eColorAttachmentOptimal };

    vk::SubpassDescription const subpass_description{ .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                                                      .colorAttachmentCount = 1,
                                                      .pColorAttachments = &attachment_reference };

    vk::SubpassDependency const subpass_dependency{ .srcSubpass = vk::SubpassExternal,
                                                    .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                                    .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                                    .srcAccessMask = vk::AccessFlagBits::eNone,
                                                    .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite };

    vk::raii::RenderPass const render_pass{ device, vk::RenderPassCreateInfo{
                                                        .attachmentCount = 1,
                                                        .pAttachments = &attachment_description,
                                                        .subpassCount = 1,
                                                        .pSubpasses = &subpass_description,
                                                        .dependencyCount = 1,
                                                        .pDependencies = &subpass_dependency } };

    vk::PipelineMultisampleStateCreateInfo const multisample_state_create_info{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = vk::False,
        .minSampleShading = 1
    };

    std::array const dynamic_states{ vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo const dynamic_state_create_info{ .dynamicStateCount =
                                                                            dynamic_states.size(),
                                                                        .pDynamicStates =
                                                                            dynamic_states.data() };

    vk::raii::Pipeline const pipeline{ device, nullptr,
                                       vk::GraphicsPipelineCreateInfo{
                                           .stageCount = shader_stage_create_infos.size(),
                                           .pStages = shader_stage_create_infos.data(),
                                           .pVertexInputState = &vertex_input_state_create_info,
                                           .pInputAssemblyState = &input_assembly_state_create_info,
                                           .pViewportState = &viewport_state_create_info,
                                           .pRasterizationState = &rasterization_state_create_info,
                                           .pMultisampleState = &multisample_state_create_info,
                                           .pColorBlendState = &color_blend_state_create_info,
                                           .pDynamicState = &dynamic_state_create_info,
                                           .layout = *layout,
                                           .renderPass = *render_pass,
                                           .subpass = 0 } };

    auto const frame_buffers_range{
        std::views::transform(image_views,
                              [&](auto const& view)
                              {
                                  return vk::raii::Framebuffer{ device, vk::FramebufferCreateInfo{
                                                                            .renderPass = render_pass,
                                                                            .attachmentCount = 1,
                                                                            .pAttachments = &*view,
                                                                            .width = extent.width,
                                                                            .height = extent.height,
                                                                            .layers = 1 } };
                              })
    };
    std::vector const frame_buffers(begin(frame_buffers_range), end(frame_buffers_range));

    vk::raii::CommandPool const command_pool{
        device, vk::CommandPoolCreateInfo{ .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                           .queueFamilyIndex = graphics_queue_family_index }
    };

    std::vector const command_buffers{ device.allocateCommandBuffers(
        vk::CommandBufferAllocateInfo{ .commandPool = command_pool,
                                       .level = vk::CommandBufferLevel::ePrimary,
                                       .commandBufferCount = 1 }) };
    auto const& command_buffer{ command_buffers.at(0) };

    vk::raii::Semaphore image_available_semaphore{ device, vk::SemaphoreCreateInfo{} };
    vk::raii::Semaphore render_finish_semaphore{ device, vk::SemaphoreCreateInfo{} };
    vk::raii::Fence fence{ device, vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled } };

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

        vk::ClearValue const color{ { { { 0.12f, 0.04f, 0.8f, 1.0f } } } };

        command_buffer.beginRenderPass({ .renderPass = render_pass,
                                         .framebuffer = frame_buffers[image_index],
                                         .renderArea = vk::Rect2D{ { 0, 0 }, extent },
                                         .clearValueCount = 1,
                                         .pClearValues = &color },
                                       vk::SubpassContents::eInline);

        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        command_buffer.setViewport(0, viewport);
        command_buffer.setScissor(0, scissor);
        command_buffer.draw(3, 1, 0, 0);

        command_buffer.endRenderPass();
        command_buffer.end();

        vk::PipelineStageFlags const stage_flag{ vk::PipelineStageFlagBits::eColorAttachmentOutput };
        graphics_queue.submit(vk::SubmitInfo{ .waitSemaphoreCount = 1,
                                              .pWaitSemaphores = &*image_available_semaphore,
                                              .pWaitDstStageMask = &stage_flag,
                                              .commandBufferCount = 1,
                                              .pCommandBuffers = &*command_buffer,
                                              .signalSemaphoreCount = 1,
                                              .pSignalSemaphores = &*render_finish_semaphore },
                              fence);

        assert(vk::Result::eSuccess == present_queue.presentKHR({ .waitSemaphoreCount = 1,
                                                                  .pWaitSemaphores = &*render_finish_semaphore,
                                                                  .swapchainCount = 1,
                                                                  .pSwapchains = &*swapchain,
                                                                  .pImageIndices = &image_index }));
    }

    return EXIT_SUCCESS;
}
