#include <binder/ProcessState.h>

namespace android {

sp<ProcessState>
ProcessState::self()
{
    return nullptr;
}

void
ProcessState::startThreadPool()
{
}

} // namespace android
