#pragma once

#include <stdint.h>

#define ANDROID_NATIVE_WINDOW_MAGIC 0x5f776e64 /* '_wnd' */

// sizeof(stub) == sizeof(real). This is required by Surface.
struct ANativeWindow {
  public:
    int magic;
    int version;

  private:
    ANativeWindow() = delete;

    [[maybe_unused]] void *pad1[6];
    [[maybe_unused]] uint32_t pad2[6];
    [[maybe_unused]] void *pad3[4];
    [[maybe_unused]] void *pad4[10];
};
