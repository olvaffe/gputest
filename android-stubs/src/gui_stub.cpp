#include <gui/LayerMetadata.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/SurfaceControl.h>

namespace android {

namespace gui {

LayerMetadata::LayerMetadata() {}

} // namespace gui

sp<SurfaceComposerClient>
SurfaceComposerClient::getDefault()
{
    return nullptr;
}

status_t
SurfaceComposerClient::initCheck() const
{
    return 0;
}

sp<SurfaceControl>
SurfaceComposerClient::createSurface(const String8 &,
                                     uint32_t,
                                     uint32_t,
                                     PixelFormat,
                                     int32_t,
                                     const sp<IBinder> &,
                                     const gui::LayerMetadata &,
                                     uint32_t *)
{
    return nullptr;
}

SurfaceComposerClient::Transaction::Transaction() {}

SurfaceComposerClient::Transaction &
SurfaceComposerClient::Transaction::setPosition(const sp<SurfaceControl> &, float, float)
{
    return *this;
}

SurfaceComposerClient::Transaction &
SurfaceComposerClient::Transaction::setLayer(const sp<SurfaceControl> &, int32_t)
{
    return *this;
}

status_t
SurfaceComposerClient::Transaction::apply(bool, bool)
{
    return 0;
}

sp<Surface>
SurfaceControl::getSurface()
{
    return nullptr;
}

} // namespace android
