#pragma once

#include <utility>

namespace android {

// sizeof(stub) == sizeof(real). All methods are inlined.
template <typename T> class sp {
  public:
    inline sp() : m_ptr(nullptr) {}

    inline sp(T *other) : m_ptr(other)
    {
        if (other)
            other->incStrong(this);
    }

    inline sp(const sp<T> &other) : m_ptr(other.m_ptr)
    {
        if (m_ptr)
            m_ptr->incStrong(this);
    }

    inline sp(sp<T> &&other) noexcept : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }

    inline ~sp() { clear(); }

    inline void clear()
    {
        if (m_ptr) {
            m_ptr->decStrong(this);
            m_ptr = nullptr;
        }
    }

    inline sp &operator=(sp<T> &&other) noexcept
    {
        clear();
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;
        return *this;
    }

    inline explicit operator bool() const { return m_ptr != nullptr; }
    inline T *get() const { return m_ptr; }
    inline T *operator->() const { return m_ptr; }

    template <typename... Args> static sp<T> make(Args &&...args)
    {
        return sp<T>(new T(std::forward<Args>(args)...));
    }

  private:
    T *m_ptr;
};

} // namespace android
