#include <orbi/ctx.hpp>
#include <orbi/device.hpp>
#include <orbi/window.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

#include <vulkan/vulkan_raii.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>

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

void
handle_nested_exceptions(std::exception const& e, int level = 0)
{
    if (level == 0)
    {
        std::cerr << "Exception was thrown:\n";
        ++level;
    }

    for (auto _ : std::views::iota(0, level)) std::cerr << "    ";
    std::cerr << "what(): " << e.what() << '\n';

    try
    {
        std::rethrow_if_nested(e);
    }
    catch (std::exception const& nested)
    {
        handle_nested_exceptions(nested, level + 1);
    }
}

int
main()
try
{
    using namespace orbi;

    ctx ctx{ ctx::subsystem::video | ctx::subsystem::event,
             { .name = "probably triangle", .semver = version{ 0, 1, 0 } } };

    window window{ ctx };
    {
        auto const ec = window.set(window::flag::resizable);
        assert(!bool(ec));
    }

    device device{ ctx, window };

    auto const& vulkan_device{ device.inner_vulkan_device() };
    auto const surface{ window.inner_vulkan_surface() };
    auto const& vulkan_physical_device{ device.inner_vulkan_physical_device() };
    auto const graphics_queue_family_index{ device.inner_vulkan_queue_family_index(device::queue_family::graphics) };
    auto const present_queue_family_index{ device.inner_vulkan_queue_family_index(device::queue_family::present) };
    std::vector const unique_queue_families = [&]
    {
        std::vector families{ graphics_queue_family_index, present_queue_family_index };
        std::ranges::sort(families);
        auto const [first, last] = std::ranges::unique(families);
        families.erase(first, last);

        return families;
    }();

    vk::raii::Queue const graphics_queue{ vulkan_device.getQueue(graphics_queue_family_index, 0) };
    vk::raii::Queue const present_queue{ vulkan_device.getQueue(present_queue_family_index, 0) };

    auto const surface_format = [&]
    {
        std::vector const formats{ vulkan_physical_device.getSurfaceFormatsKHR(surface) };

        auto const fmt{ std::ranges::find(formats, vk::SurfaceFormatKHR{ vk::Format::eB8G8R8A8Srgb,
                                                                         vk::ColorSpaceKHR::eSrgbNonlinear }) };

        assert(fmt != end(formats));
        return *fmt;
    }();

    auto const surface_present_mode = [&]
    {
        std::vector const modes{ vulkan_physical_device.getSurfacePresentModesKHR(surface) };
        auto const mode{ std::ranges::find(modes, vk::PresentModeKHR::eFifo) };

        assert(mode != end(modes));
        return *mode;
    }();

    vk::Extent2D const default_extent{ 500, 500 };

    auto const make_extent = [&]() -> vk::Extent2D
    {
        auto const surface_capabilities{ vulkan_physical_device.getSurfaceCapabilitiesKHR(surface) };
        vk::Extent2D extent = surface_capabilities.currentExtent;
        bool const is_extent_should_be_determined_by_swapchain =
            extent == vk::Extent2D{ 0xFFFFFFFF, 0xFFFFFFFF };

        if (is_extent_should_be_determined_by_swapchain)
        {
            extent = default_extent;
        } else
        {
            assert(extent.height >= surface_capabilities.minImageExtent.height);
            assert(extent.height <= surface_capabilities.maxImageExtent.height);
            assert(extent.width >= surface_capabilities.minImageExtent.width);
            assert(extent.width <= surface_capabilities.maxImageExtent.width);
        }

        return extent;
    };

    vk::Extent2D extent{ make_extent() };

    auto const make_swapchain = [&]() -> vk::raii::SwapchainKHR
    {
        auto const surface_capabilities{ vulkan_physical_device.getSurfaceCapabilitiesKHR(surface) };
        auto const max_image_count = surface_capabilities.maxImageCount == 0
                                         ? std::numeric_limits<std::uint32_t>::max()
                                         : surface_capabilities.maxImageCount;

        auto const image_count = std::clamp(3u, surface_capabilities.minImageCount, max_image_count);

        auto const sharing_mode = unique_queue_families.size() == 1 ? vk::SharingMode::eExclusive
                                                                    : vk::SharingMode::eConcurrent;

        return { vulkan_device,
                 { .surface = surface,
                   .minImageCount = image_count,
                   .imageFormat = surface_format.format,
                   .imageColorSpace = surface_format.colorSpace,
                   .imageExtent = extent,
                   .imageArrayLayers = 1,
                   .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
                   .imageSharingMode = sharing_mode,
                   .queueFamilyIndexCount = static_cast<std::uint32_t>(unique_queue_families.size()),
                   .pQueueFamilyIndices = unique_queue_families.data(),
                   .preTransform = surface_capabilities.currentTransform,
                   .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
                   .presentMode = surface_present_mode,
                   .clipped = vk::True } };
    };
    vk::raii::SwapchainKHR swapchain{ make_swapchain() };

    auto const res_dir{ std::filesystem::current_path() / "example/triangle/res" };
    auto const vertex_shader_bytecode{ read_file(res_dir / "triangle.vert.spv") };
    auto const fragment_shader_bytecode{ read_file(res_dir / "triangle.frag.spv") };

    vk::raii::ShaderModule const vertex_shader{ vulkan_device,
                                                { .codeSize = vertex_shader_bytecode.size(),
                                                  .pCode = reinterpret_cast<std::uint32_t const*>(
                                                      vertex_shader_bytecode.data()) } };

    vk::raii::ShaderModule const fragment_shader{ vulkan_device,
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

    vk::raii::PipelineLayout const layout{ vulkan_device, vk::PipelineLayoutCreateInfo{} };

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

    vk::raii::RenderPass const render_pass{ vulkan_device, vk::RenderPassCreateInfo{
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

    vk::raii::Pipeline const pipeline{ vulkan_device, nullptr,
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

    auto const make_image_views = [&]
    {
        auto const rng = swapchain.getImages() |
                         std::views::transform(
                             [&](auto const& image) -> vk::raii::ImageView
                             {
                                 return { vulkan_device,
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
    };

    std::vector image_views{ make_image_views() };

    auto const make_frame_buffers = [&]
    {
        auto const rng{ image_views | std::views::transform(
                                          [&](auto const& view) -> vk::raii::Framebuffer
                                          {
                                              return { vulkan_device, vk::FramebufferCreateInfo{
                                                                          .renderPass = render_pass,
                                                                          .attachmentCount = 1,
                                                                          .pAttachments = &*view,
                                                                          .width = extent.width,
                                                                          .height = extent.height,
                                                                          .layers = 1 } };
                                          }) };

        return std::vector(begin(rng), end(rng));
    };

    std::vector frame_buffers{ make_frame_buffers() };

    vk::raii::CommandPool const command_pool{
        vulkan_device, vk::CommandPoolCreateInfo{ .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                                  .queueFamilyIndex = graphics_queue_family_index }
    };

    std::vector const command_buffers{ vulkan_device.allocateCommandBuffers(
        vk::CommandBufferAllocateInfo{ .commandPool = command_pool,
                                       .level = vk::CommandBufferLevel::ePrimary,
                                       .commandBufferCount = 1 }) };
    auto const& command_buffer{ command_buffers.at(0) };

    vk::raii::Semaphore image_available_semaphore{ vulkan_device, vk::SemaphoreCreateInfo{} };
    vk::raii::Semaphore render_finish_semaphore{ vulkan_device, vk::SemaphoreCreateInfo{} };
    vk::raii::Fence fence{ vulkan_device, vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled } };

    while (true)
    {
        auto constexpr timeout{ 3'000'000'000 /* std::numeric_limits<std::uint64_t>::max() */ };

        {
            [[maybe_unused]] auto const result{ vulkan_device.waitForFences(*fence, vk::True, timeout) };
            assert(vk::Result::eSuccess == result);
        }

        vulkan_device.resetFences(*fence);

        SDL_Event ev{};
        while (SDL_PollEvent(&ev))
        {
            switch (static_cast<SDL_EventType>(ev.type))
            {
            case SDL_EVENT_QUIT:
                return EXIT_SUCCESS;

            case SDL_EVENT_WINDOW_RESIZED:
                extent = make_extent();
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

    vulkan_device.waitIdle();

    return EXIT_SUCCESS;
}
catch (std::exception const& e)
{
    handle_nested_exceptions(e);
}
