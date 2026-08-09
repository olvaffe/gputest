#pragma once

#include <gui/Surface.h>
#include <utils/RefBase.h>
#include <utils/StrongPointer.h>

namespace android {

// memory layout: truncated but not directly constructed/destructed/accessed
// vtable: truncated
// methods: resolved to libgui
class SurfaceControl : public RefBase {
  public:
    sp<Surface> getSurface();

  private:
    SurfaceControl() = delete;
    ~SurfaceControl() override = default;
};

} // namespace android
