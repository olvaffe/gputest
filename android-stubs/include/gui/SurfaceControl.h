#pragma once

#include <gui/Surface.h>
#include <utils/RefBase.h>
#include <utils/StrongPointer.h>

namespace android {

// This is not directly constructible. All methods are defined in libgui.
class SurfaceControl : public RefBase {
  public:
    ~SurfaceControl();
    sp<Surface> getSurface();

  private:
    SurfaceControl() = delete;
};

} // namespace android
