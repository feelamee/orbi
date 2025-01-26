#pragma once

#include <orbi/detail/pimpl.hpp>
#include <orbi/detail/util.hpp>

#include <vulkan/vulkan_raii.hpp>

#include <cstdint>

namespace orbi
{

struct window;
struct ctx;

struct device : detail::noncopyable
{
    device(ctx const&, window const&);
    ~device();

    device(device&&);
    device& operator=(device);

    device& operator=(device&&) = delete;

    friend void swap(device&, device&) noexcept;

    // TODO remove; created only to postpone design of api
    vk::raii::PhysicalDevice const& inner_vulkan_physical_device() const;
    vk::raii::Device const& inner_vulkan_device() const;

    enum class queue_family
    {
        graphics,
        present,
    };

    using queue_family_index_type = std::uint32_t;
    queue_family_index_type inner_vulkan_queue_family_index(queue_family) const;

private:
    struct impl;

    detail::pimpl<impl, 48, 8> data;
};

} // namespace orbi
