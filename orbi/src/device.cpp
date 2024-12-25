#include <orbi/ctx.hpp>
#include <orbi/device.hpp>
#include <orbi/window.hpp>

#include <vulkan/vulkan_raii.hpp>

#include <algorithm>
#include <cstdint>

namespace orbi
{

struct device::impl
{
    vk::raii::PhysicalDevice physical_device{ nullptr };
    vk::raii::Device device{ nullptr };

    queue_family_index_type graphics_queue_family_index{ 0 };
    queue_family_index_type present_queue_family_index{ 0 };
};

device::device(ctx const& ctx, window const& win)
{
    assert(ctx.inited_subsystems() & ctx::subsystem::video);
    auto const& vulkan_instance{ *ctx.inner_vulkan_instance() };

    pimpl->physical_device = vulkan_instance.enumeratePhysicalDevices().at(0);

    pimpl->graphics_queue_family_index = [&]()
    {
        auto const queue_families{ pimpl->physical_device.getQueueFamilyProperties() };
        auto const it{ std::ranges::find_if(queue_families,
                                            [](auto const props) {
                                                return static_cast<bool>(props.queueFlags &
                                                                         vk::QueueFlagBits::eGraphics);
                                            }) };

        assert(it != end(queue_families));

        return it - begin(queue_families);
    }();

    auto const surface{ win.inner_vulkan_surface() };

    pimpl->present_queue_family_index = [&]()
    {
        auto const queue_families{ pimpl->physical_device.getQueueFamilyProperties() };
        auto const it{ std::ranges::find_if(queue_families,
                                            [&, i = 0](auto const) mutable {
                                                return pimpl->physical_device.getSurfaceSupportKHR(i++, surface);
                                            }) };

        assert(it != end(queue_families));

        return it - begin(queue_families);
    }();

    std::vector const unique_queue_families = [&]
    {
        std::vector families{ pimpl->graphics_queue_family_index, pimpl->present_queue_family_index };
        std::ranges::sort(families);
        auto const [first, last] = std::ranges::unique(families);
        families.erase(first, last);

        return families;
    }();

    pimpl->device = [&]() -> vk::raii::Device
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

        return { pimpl->physical_device, device_create_info };
    }();
}

device::~device()
{
}

device::device(device&& other)
{
    pimpl->physical_device = std::exchange(other.pimpl->physical_device, nullptr);
    pimpl->device = std::exchange(other.pimpl->device, nullptr);
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

    swap(*l.pimpl, *r.pimpl);
}

vk::raii::PhysicalDevice const&
device::inner_vulkan_physical_device() const
{
    return pimpl->physical_device;
}

vk::raii::Device const&
device::inner_vulkan_device() const
{
    return pimpl->device;
}

device::queue_family_index_type
device::inner_vulkan_queue_family_index(queue_family family) const
{
    switch (family)
    {
    case queue_family::graphics:
        return pimpl->graphics_queue_family_index;

    case queue_family::present:
        return pimpl->present_queue_family_index;
    }

    detail::unreachable();
}

} // namespace orbi
