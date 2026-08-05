#pragma once

#include <utils/RefBase.h>

namespace android {

// This is not directly constructible. Needed only for name mangling.
class IBinder : public RefBase {
  private:
    IBinder() = delete;
};

} // namespace android
