#pragma once

#include <orbi/detail/pimpl.hpp>
#include <orbi/detail/util.hpp>

#include <vulkan/vulkan_raii.hpp>

namespace orbi
{

struct window;
struct context;

struct device
{
    device(context const&, window const&);
    ~device();

    device(device&&) noexcept;
    device& operator=(device);

    friend void swap(device&, device&) noexcept;

    void wait_until_idle() const;

    struct impl;
    friend impl;

private:
    detail::pimpl<impl, 48, 8> data;
};

} // namespace orbi
