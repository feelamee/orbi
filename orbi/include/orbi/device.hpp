#pragma once

#include <orbi/detail/pimpl.hpp>
#include <orbi/detail/util.hpp>

#include <vulkan/vulkan_raii.hpp>

namespace orbi
{

struct window;
struct ctx;

struct device
{
    device(ctx const&, window const&);
    ~device();

    device(device&&) noexcept;
    device& operator=(device);

    friend void swap(device&, device&) noexcept;

    struct impl;
    friend impl;

private:
    detail::pimpl<impl, 48, 8> data;
};

} // namespace orbi
