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

// memory layout: truncated but not directly constructed/destructed/accessed
// vtable: truncated
// methods: resolved to libgui
class SurfaceComposerClient : public RefBase {
  public:
    status_t initCheck() const;

    static sp<SurfaceComposerClient> getDefault();

    sp<SurfaceControl> createSurface(const String8 &name,
                                     uint32_t w,
                                     uint32_t h,
                                     PixelFormat format,
                                     int32_t flags = 0,
                                     const sp<IBinder> &parentHandle = nullptr,
                                     const LayerMetadata &metadata = LayerMetadata(),
                                     uint32_t *outTransformHint = nullptr);

    // memory layout: incompatible but padded to have enough storage
    // vtable: omitted and ignored
    // methods: resolved to libgui
    class Transaction {
      public:
        Transaction();

        Transaction &setPosition(const sp<SurfaceControl> &sc, float x, float y);
        Transaction &setLayer(const sp<SurfaceControl> &sc, int32_t z);
        status_t apply(bool synchronous = false, bool oneWay = false);

      private:
        [[maybe_unused]] char pad[1024];
    };

  private:
    SurfaceComposerClient() = delete;
    ~SurfaceComposerClient() override = default;
};

} // namespace android
