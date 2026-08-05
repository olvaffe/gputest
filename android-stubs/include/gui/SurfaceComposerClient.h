#pragma once

#include <binder/IBinder.h>
#include <gui/LayerMetadata.h>
#include <gui/SurfaceControl.h>
#include <stdint.h>
#include <ui/PixelFormat.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/String8.h>
#include <utils/StrongPointer.h>

namespace android {

using gui::LayerMetadata;

// The padding is chosen such that sizeof(stub) >= sizeof(real). All methods
// are defined in libgui.
class SurfaceComposerClient : public RefBase {
  public:
    SurfaceComposerClient();
    virtual ~SurfaceComposerClient();

    status_t initCheck() const;
    sp<SurfaceControl> createSurface(const String8 &name,
                                     uint32_t w,
                                     uint32_t h,
                                     PixelFormat format,
                                     int32_t flags = 0,
                                     const sp<IBinder> &parentHandle = nullptr,
                                     const LayerMetadata &metadata = LayerMetadata(),
                                     uint32_t *outTransformHint = nullptr);

    // The padding is chosen such that sizeof(stub) >= sizeof(real). All methods
    // are defined in libgui.
    class Transaction {
      public:
        Transaction();
        Transaction &setLayer(const sp<SurfaceControl> &sc, int32_t z);
        Transaction &show(const sp<SurfaceControl> &sc);
        status_t apply(bool synchronous = false, bool oneWay = false);

      private:
        [[maybe_unused]] char mPadding[1024];
    };

  private:
    [[maybe_unused]] char mPadding[256];
};

} // namespace android
