#pragma once

namespace android {

// memory layout: compatible but not directly constructed/destructed/accessed
// vtable: truncated
// methods: resolved to libutils
class RefBase {
  public:
    void incStrong(const void *id) const;
    void decStrong(const void *id) const;

  protected:
    virtual ~RefBase() = default;

  private:
    RefBase() = delete;
    [[maybe_unused]] void *pad;
};

} // namespace android
