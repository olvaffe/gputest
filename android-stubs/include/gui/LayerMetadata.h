#pragma once

namespace android {
namespace gui {

// memory layout: incompatible but padded to have enough storage
// vtable: omitted and ignored
// methods: resolved to libgui
struct LayerMetadata {
  public:
    LayerMetadata();

  private:
    [[maybe_unused]] char pad[128];
};

} // namespace gui
} // namespace android
