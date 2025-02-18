#pragma once

#include <array>

namespace orbi::detail
{

template <class T, std::size_t Size, std::size_t Alignment>
struct pimpl
{
public:
    template <class... Args>
    pimpl(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        std::construct_at(reinterpret_cast<T*>(data.data()), std::forward<Args>(args)...);
    }

    ~pimpl()
    {
        validate();

        std::destroy_at(as_held());
    }

    pimpl(pimpl const& other) noexcept(noexcept(pimpl(*other)))
        : pimpl(*other)
    {
    }

    pimpl(pimpl&& other) noexcept(noexcept(pimpl(std::move(*other))))
        : pimpl(std::move(*other))
    {
    }

    pimpl& operator=(pimpl&&) = delete;

    pimpl&
    operator=(pimpl other) noexcept(noexcept(*as_held() = std::move(*other)))
    {
        *as_held() = std::move(*other);
    }

    friend void
    swap(pimpl& l, pimpl& r) noexcept
    {
        using std::swap;

        swap(l.data, r.data);
    }

    consteval static void
    validate()
    {
        static_assert(Size == sizeof(T));
        static_assert(Alignment == alignof(T));
    }

    T*
    operator->() noexcept
    {
        return as_held();
    }

    T const*
    operator->() const noexcept
    {
        return as_held();
    }

    T&
    operator*() noexcept
    {
        return *as_held();
    }

    T const&
    operator*() const noexcept
    {
        return *as_held();
    }

private:
    T const*
    as_held() const noexcept
    {
        return reinterpret_cast<T const*>(data.data());
    }

    T*
    as_held() noexcept
    {
        return reinterpret_cast<T*>(data.data());
    }

    alignas(Alignment) std::array<std::byte, Size> data;
};

} // namespace orbi::detail
