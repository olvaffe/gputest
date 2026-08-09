#pragma once

#include <system/window.h>
#include <utils/RefBase.h>

namespace android {

// memory layout: truncated but not directly constructed/destructed
// vtable: truncated
// methods: none exposed
class Surface : public ANativeWindow, public RefBase {
  private:
    Surface() = delete;
    ~Surface() override = default;
};

} // namespace android
