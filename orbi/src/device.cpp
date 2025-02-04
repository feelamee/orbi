#include <orbi/context.hpp>
#include <orbi/detail/impl.hpp>
#include <orbi/device.hpp>
#include <orbi/window.hpp>

#include <vulkan/vulkan_raii.hpp>

#include <algorithm>

namespace orbi
{

device::device(context const& ctx, window const& win)
{
    auto const& ctx_impl{ context::impl::from_ctx(ctx) };
    auto const& win_impl{ window::impl::from_window(win) };
    auto const& vulkan_instance{ ctx_impl.vulkan_instance };

    data->vk_physical_device =
        vk::raii::PhysicalDevice(vulkan_instance, *vulkan_instance.enumeratePhysicalDevices().at(0));

    data->graphics_queue_family_index = [&]
    {
        auto const queue_families{ data->vk_physical_device.getQueueFamilyProperties() };
        auto const it{ std::ranges::find_if(queue_families,
                                            [](auto const props)
                                            {
                                                return static_cast<bool>(props.queueFlags &
                                                                         vk::QueueFlagBits::eGraphics);
                                            }) };

        assert(it != end(queue_families));

        return it - begin(queue_families);
    }();

    auto* const surface{ win_impl.vk_surface };

    data->present_queue_family_index = [&]
    {
        auto const queue_families{ data->vk_physical_device.getQueueFamilyProperties() };
        auto const it{ std::ranges::find_if(queue_families,
                                            [&, i = 0](auto const) mutable
                                            {
                                                return data->vk_physical_device.getSurfaceSupportKHR(i++, surface);
                                            }) };

        assert(it != end(queue_families));

        return it - begin(queue_families);
    }();

    std::vector const unique_queue_families = [&]
    {
        std::vector families{ data->graphics_queue_family_index, data->present_queue_family_index };
        std::ranges::sort(families);
        auto const [first, last] = std::ranges::unique(families);
        families.erase(first, last);

        return families;
    }();

    data->vk_device = [&]() -> vk::raii::Device
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

        return { data->vk_physical_device, device_create_info };
    }();
}

device::~device() = default;

device::device(device&& other) noexcept
{
    data->vk_physical_device = std::exchange(other.data->vk_physical_device, nullptr);
    data->vk_device = std::exchange(other.data->vk_device, nullptr);
}

device&
device::operator=(device other)
{
    swap(*this, other);

    return *this;
}

void
swap(device& l, device& r) noexcept
{
    using std::swap;

    swap(*l.data, *r.data);
}

} // namespace orbi
