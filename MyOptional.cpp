// MyOptional.hpp
#pragma once
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>
#include <cassert>

template <typename T>
class MyOptional {
private:
    bool m_has = false;
    alignas(T) std::byte m_buf[sizeof(T)];

    T*       ptr_unchecked()       noexcept { return std::launder(reinterpret_cast<T*>(m_buf)); }
    const T* ptr_unchecked() const noexcept { return std::launder(reinterpret_cast<const T*>(m_buf)); }

    void destroy_if() noexcept(std::is_nothrow_destructible_v<T>) {
        if (m_has) {
            ptr_unchecked()->~T();
            m_has = false;
        }
    }

public:
    MyOptional() noexcept = default;

    ~MyOptional() noexcept(std::is_nothrow_destructible_v<T>) {
        destroy_if();
    }

    explicit MyOptional(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>) {
        ::new (m_buf) T(value);
        m_has = true;
    }
    explicit MyOptional(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>) {
        ::new (m_buf) T(std::move(value));
        m_has = true;
    }

    MyOptional(const MyOptional& other) noexcept(std::is_nothrow_copy_constructible_v<T>) {
        if (other.m_has) {
            ::new (m_buf) T(*other.ptr_unchecked());
            m_has = true;
        }
    }

    MyOptional(MyOptional&& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
        if (other.m_has) {
            ::new (m_buf) T(std::move(*other.ptr_unchecked()));
            m_has = true;
        }
    }

    MyOptional& operator=(const MyOptional& other)
        noexcept(std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_swappable_v<T> &&
                 std::is_nothrow_destructible_v<T>) {
        if (this == &other) return *this;

        if (m_has && other.m_has) {
            T tmp(*other.ptr_unchecked());
            using std::swap;
            swap(*ptr_unchecked(), tmp);
        } else if (m_has && !other.m_has) {
            destroy_if();
        } else if (!m_has && other.m_has) {
            ::new (m_buf) T(*other.ptr_unchecked());
            m_has = true;
        }
        return *this;
    }

    MyOptional& operator=(MyOptional&& other)
        noexcept(std::is_nothrow_move_assignable_v<T> && std::is_nothrow_move_constructible_v<T> &&
                 std::is_nothrow_swappable_v<T> && std::is_nothrow_destructible_v<T>) {
        if (this == &other) return *this;

        if (m_has && other.m_has) {
            if constexpr (std::is_nothrow_move_assignable_v<T>) {
                *ptr_unchecked() = std::move(*other.ptr_unchecked());
            } else {
                T tmp(std::move(*other.ptr_unchecked()));
                using std::swap;
                swap(*ptr_unchecked(), tmp);
            }
        } else if (m_has && !other.m_has) {
            destroy_if();
        } else if (!m_has && other.m_has) {
            ::new (m_buf) T(std::move(*other.ptr_unchecked()));
            m_has = true;
        }
        return *this;
    }

    void reset() noexcept(std::is_nothrow_destructible_v<T>) { destroy_if(); }

    template <class... Args>
    T& emplace(Args&&... args) {
        if (!m_has) {
            ::new (m_buf) T(std::forward<Args>(args)...);
            m_has = true;
            return *ptr_unchecked();
        }
        T tmp(std::forward<Args>(args)...);
        using std::swap;
        swap(*ptr_unchecked(), tmp);
        return *ptr_unchecked();
    }

    bool has_value() const noexcept { return m_has; }
    explicit operator bool() const noexcept { return m_has; }

    T&       value()       { assert(m_has); return *ptr_unchecked(); }
    const T& value() const { assert(m_has); return *ptr_unchecked(); }

    T*       operator->()       { assert(m_has); return ptr_unchecked(); }
    const T* operator->() const { assert(m_has); return ptr_unchecked(); }

    T&       operator*()       { assert(m_has); return *ptr_unchecked(); }
    const T& operator*() const { assert(m_has); return *ptr_unchecked(); }

    template <class U>
    T value_or(U&& fallback) const {
        return m_has ? *ptr_unchecked() : T(std::forward<U>(fallback));
    }

    void swap(MyOptional& o) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                      std::is_nothrow_swappable_v<T>) {
        using std::swap;
        if (m_has && o.m_has) {
            swap(*ptr_unchecked(), *o.ptr_unchecked());
        } else if (m_has && !o.m_has) {
            o.emplace(std::move(*ptr_unchecked()));
            destroy_if();
        } else if (!m_has && o.m_has) {
            emplace(std::move(*o.ptr_unchecked()));
            o.destroy_if();
        }
    }
};

template <class T>
void swap(MyOptional<T>& a, MyOptional<T>& b) noexcept(noexcept(a.swap(b))) {
    a.swap(b);
}
