#pragma once

#include <utils/RefBase.h>

namespace android {

// memory layout: compatible but not directly constructed/destructed/accessed
// vtable: truncated
// methods: none exposed
class IBinder : public virtual RefBase {
  private:
    IBinder() = delete;
    ~IBinder() override = default;
};

} // namespace android
