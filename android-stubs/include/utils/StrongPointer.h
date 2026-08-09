#pragma once

#include <cstddef>

namespace android {

// memory layout: compatible
// vtable: none
// methods: inlined
template <typename T> class sp {
  public:
    constexpr sp() : m_ptr(nullptr) {}
    sp(std::nullptr_t) : sp() {}

    sp(const sp<T> &other) : m_ptr(other.m_ptr)
    {
        if (m_ptr)
            m_ptr->incStrong(this);
    }
    sp(sp<T> &&other) noexcept : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }

    ~sp()
    {
        if (m_ptr)
            m_ptr->decStrong(this);
    }

    void clear()
    {
        if (m_ptr) {
            m_ptr->decStrong(this);
            m_ptr = nullptr;
        }
    }

    sp &operator=(const sp<T> &other)
    {
        if (other.m_ptr)
            other.m_ptr->incStrong(this);
        if (m_ptr)
            m_ptr->decStrong(this);
        m_ptr = other.m_ptr;
        return *this;
    }

    sp &operator=(sp<T> &&other) noexcept
    {
        if (m_ptr)
            m_ptr->decStrong(this);
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;
        return *this;
    }

    explicit operator bool() const { return m_ptr != nullptr; }
    T *get() const { return m_ptr; }
    T *operator->() const { return m_ptr; }

  private:
    T *m_ptr;
};

} // namespace android
