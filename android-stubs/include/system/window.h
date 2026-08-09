#pragma once

#define ANDROID_NATIVE_WINDOW_MAGIC 0x5f776e64 /* '_wnd' */

// memory layout: truncated but not directly constructed/destructed
// vtable: none
// methods: none
struct ANativeWindow {
  public:
    int magic;

  private:
    ANativeWindow() = delete;
};
