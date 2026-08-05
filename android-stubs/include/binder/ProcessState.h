#pragma once

#include <utils/RefBase.h>
#include <utils/StrongPointer.h>

namespace android {

// This is not directly constructible. All methods are defined in libbinder.
class ProcessState : public virtual RefBase {
  public:
    static sp<ProcessState> self();
    void startThreadPool();

  private:
    ProcessState() = delete;
};

} // namespace android
