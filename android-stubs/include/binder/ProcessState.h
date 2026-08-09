#pragma once

#include <utils/RefBase.h>
#include <utils/StrongPointer.h>

namespace android {

// memory layout: truncated but not directly constructed/destructed/accessed
// vtable: compatible
// methods: resolved to libbinder
class ProcessState : public virtual RefBase {
  public:
    static sp<ProcessState> self();
    void startThreadPool();

  private:
    ProcessState() = delete;
    ~ProcessState() override = default;
};

} // namespace android
