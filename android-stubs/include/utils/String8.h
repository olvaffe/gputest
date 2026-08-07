#pragma once

namespace android {

// sizeof(stub) == sizeof(real). All methods are defined in libutils.
class String8 {
  public:
    String8(const char *o);
    ~String8();

  private:
    [[maybe_unused]] void *pad;
};

} // namespace android
