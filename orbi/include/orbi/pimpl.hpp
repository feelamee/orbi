#pragma once

#include <array>

template <class T, std::size_t Size, std::size_t Alignment>
struct pimpl
{
public:
    template <class... Args>
    pimpl(Args&&... args)
    {
        std::construct_at(data.data(), std::forward<Args>(args)...);
    }

    ~pimpl()
    {
        validate();

        std::destroy_at(data.data());
    }

    pimpl(pimpl const& other)
        : pimpl(*other)
    {
    }

    pimpl(pimpl&& other)
        : pimpl(*other)
    {
    }

    pimpl& operator=(pimpl&&) = delete;

    pimpl&
    operator=(pimpl other)
    {
        *as_held() = std::move(*other);
    }

    friend void
    swap(pimpl& l, pimpl& r)
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

    alignas(Alignment) std::array<std::byte, Size> data;
};
