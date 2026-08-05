#pragma once

#include <system/window.h>
#include <utils/RefBase.h>

namespace android {

// This is not directly constructible. But because ANativeWindow is before
// RefBase, sizeof(stub) and sizeof(real) must be the same for ANativeWindow.
class Surface : public ANativeWindow, public RefBase {
  private:
    Surface() = delete;
};

} // namespace android
