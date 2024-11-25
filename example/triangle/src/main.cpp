#include <orbi/ctx.hpp>
#include <orbi/window.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

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

    auto const size{ std::filesystem::file_size(filename) };
    std::vector<std::byte> buffer{ size };

    file.read(reinterpret_cast<std::ifstream::char_type*>(buffer.data()), static_cast<std::streamsize>(size));

    return buffer;
}

int
main()
{
    orbi::ctx ctx{ orbi::ctx::subsystem::video | orbi::ctx::subsystem::event,
                   { .name = "probably triangle", .version = vk::makeApiVersion(0, 0, 1, 0) } };

    orbi::window window{ ctx };

    // so.. bad code should be ugly)
    auto& vulkan_instance{
        std::any_cast<std::reference_wrapper<vk::raii::Instance>>(ctx.inner_vulkan_instance()).get()
    };

    auto const debug_utils_messenger{
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

    VkSurfaceKHR surface{};
    if (!SDL_Vulkan_CreateSurface(std::any_cast<SDL_Window*>(window.inner()), *vulkan_instance, nullptr, &surface))
    {
        throw sdl_error{ std::format("SDL_Vulkan_CreateSurface failed with: '{}'", SDL_GetError()) };
    }
    scope_exit const destroy_surface{ [&] {
        SDL_Vulkan_DestroySurface(*vulkan_instance, surface, nullptr);
    } };

    vk::raii::PhysicalDevice const physical_device{ vulkan_instance.enumeratePhysicalDevices().at(0) };

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
                                     { return physical_device.getSurfaceSupportKHR(i++, surface); })
            };

            assert(it != end(queue_families));

            return it - begin(queue_families);
        }()
    };

    std::vector const unique_queue_families{ [&]
                                             {
                                                 std::vector families{ graphics_queue_family_index,
                                                                       present_queue_family_index };
                                                 std::ranges::sort(families);
                                                 auto const [first, last] = std::ranges::unique(families);
                                                 families.erase(first, last);

                                                 return families;
                                             }() };

    auto const device{ [&]() -> vk::raii::Device
                       {
                           float const queue_priority{ 1 };

                           std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
                           for (auto const& qf : unique_queue_families)
                           {
                               queue_create_infos.push_back(
                                   { .queueFamilyIndex = qf, .queueCount = 1, .pQueuePriorities = &queue_priority });
                           }

                           std::array const device_extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

                           vk::DeviceCreateInfo const device_create_info{
                               .queueCreateInfoCount = static_cast<std::uint32_t>(queue_create_infos.size()),
                               .pQueueCreateInfos = queue_create_infos.data(),
                               .enabledExtensionCount = device_extensions.size(),
                               .ppEnabledExtensionNames = device_extensions.data()

                           };

                           return { physical_device, device_create_info };
                       }() };

    vk::raii::Queue const graphics_queue{ device.getQueue(graphics_queue_family_index, 0) };
    vk::raii::Queue const present_queue{ device.getQueue(present_queue_family_index, 0) };

    auto const surface_format{
        [&]
        {
            std::vector const formats{ physical_device.getSurfaceFormatsKHR(surface) };
            auto const fmt{ std::ranges::find(formats, vk::SurfaceFormatKHR{ vk::Format::eB8G8R8A8Srgb,
                                                                             vk::ColorSpaceKHR::eSrgbNonlinear }) };

            assert(fmt != end(formats));
            return *fmt;
        }()
    };

    auto const surface_present_mode{
        [&]
        {
            std::vector const modes{ physical_device.getSurfacePresentModesKHR(surface) };
            auto const mode{ std::ranges::find(modes, vk::PresentModeKHR::eFifo) };

            assert(mode != end(modes));
            return *mode;
        }()
    };

    auto const surface_capabilities{ physical_device.getSurfaceCapabilitiesKHR(surface) };
    vk::Extent2D extent{ surface_capabilities.currentExtent };

    auto const make_swapchain{
        [&]() -> vk::raii::SwapchainKHR
        {
            return { device,
                     { .surface = surface,
                       .minImageCount = std::clamp(3u, surface_capabilities.minImageCount,
                                                   surface_capabilities.maxImageCount == 0
                                                       ? std::numeric_limits<std::uint32_t>::max()
                                                       : surface_capabilities.maxImageCount),
                       .imageFormat = surface_format.format,
                       .imageColorSpace = surface_format.colorSpace,
                       .imageExtent = extent,
                       .imageArrayLayers = 1,
                       .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
                       .imageSharingMode = unique_queue_families.size() == 1 ? vk::SharingMode::eExclusive
                                                                             : vk::SharingMode::eConcurrent,
                       .queueFamilyIndexCount = static_cast<std::uint32_t>(unique_queue_families.size()),
                       .pQueueFamilyIndices = unique_queue_families.data(),
                       .preTransform = surface_capabilities.currentTransform,
                       .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
                       .presentMode = surface_present_mode,
                       .clipped = vk::True } };
        }
    };
    vk::raii::SwapchainKHR swapchain{ make_swapchain() };

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

    vk::PipelineViewportStateCreateInfo const viewport_state_create_info{
        .viewportCount = 1,
        .scissorCount = 1,
    };

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

    vk::AttachmentDescription const attachment_description{ .format = surface_format.format,
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

    auto const make_image_views{
        [&]
        {
            auto const rng =
                swapchain.getImages() |
                std::views::transform(
                    [&](auto const& image) -> vk::raii::ImageView
                    {
                        return { device,
                                 { .image = image,
                                   .viewType = vk::ImageViewType::e2D,
                                   .format = surface_format.format,
                                   .subresourceRange = { .aspectMask = vk::ImageAspectFlagBits::eColor,
                                                         .baseMipLevel = 0,
                                                         .levelCount = 1,
                                                         .baseArrayLayer = 0,
                                                         .layerCount = 1 } } };
                    });

            return std::vector(begin(rng), end(rng));
        }
    };

    std::vector image_views{ make_image_views() };

    auto const make_frame_buffers{
        [&]
        {
            auto const rng{ image_views |
                            std::views::transform(
                                [&](auto const& view) -> vk::raii::Framebuffer
                                {
                                    return { device, vk::FramebufferCreateInfo{ .renderPass = render_pass,
                                                                                .attachmentCount = 1,
                                                                                .pAttachments = &*view,
                                                                                .width = extent.width,
                                                                                .height = extent.height,
                                                                                .layers = 1 } };
                                }) };

            return std::vector(begin(rng), end(rng));
        }

    };

    std::vector frame_buffers{ make_frame_buffers() };

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

        {
            [[maybe_unused]] auto const result{ device.waitForFences(*fence, vk::True, timeout) };
            assert(vk::Result::eSuccess == result);
        }

        device.resetFences(*fence);

        SDL_Event ev{};
        while (SDL_PollEvent(&ev))
        {
            switch (static_cast<SDL_EventType>(ev.type))
            {
            case SDL_EVENT_QUIT:
                return EXIT_SUCCESS;

            case SDL_EVENT_WINDOW_RESIZED:
                extent = physical_device.getSurfaceCapabilitiesKHR(surface).currentExtent;
                swapchain = make_swapchain();
                image_views = make_image_views();
                frame_buffers = make_frame_buffers();
                break;

            default:
                break;
            }
        }

        [[maybe_unused]] auto const [result, image_index]{ swapchain.acquireNextImage(timeout, image_available_semaphore) };
        assert(vk::Result::eSuccess == result || vk::Result::eSuboptimalKHR == result);

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
        command_buffer.setViewport(0, vk::Viewport{ .x = 0,
                                                    .y = 0,
                                                    .width = static_cast<float>(extent.width),
                                                    .height = static_cast<float>(extent.height),
                                                    .minDepth = 0,
                                                    .maxDepth = 1 });

        command_buffer.setScissor(0, vk::Rect2D{ .offset = { 0, 0 }, .extent = extent });
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

        [[maybe_unused]] auto const result2 =
            present_queue.presentKHR({ .waitSemaphoreCount = 1,
                                       .pWaitSemaphores = &*render_finish_semaphore,
                                       .swapchainCount = 1,
                                       .pSwapchains = &*swapchain,
                                       .pImageIndices = &image_index });
        assert(vk::Result::eSuccess == result2 || vk::Result::eSuboptimalKHR == result2);
    }

    return EXIT_SUCCESS;
}
