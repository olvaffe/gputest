#pragma once

namespace android {

// memory layout: compatible
// vtable: none
// methods: resolved to libutils
class String8 {
  public:
    String8(const char *o);
    ~String8();

  private:
    [[maybe_unused]] void *pad;
};

} // namespace android
