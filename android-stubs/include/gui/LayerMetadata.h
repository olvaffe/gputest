#pragma once

namespace android {
namespace gui {

// The padding is chosen such that sizeof(stub) >= sizeof(real). All methods
// are defined in libgui.
struct LayerMetadata {
  public:
    LayerMetadata();

  private:
    [[maybe_unused]] char pad[128];
};

} // namespace gui
} // namespace android
