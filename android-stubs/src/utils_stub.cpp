#include <utils/RefBase.h>
#include <utils/String8.h>

namespace android {

void
RefBase::incStrong(const void *) const
{
}

void
RefBase::decStrong(const void *) const
{
}

String8::String8(const char *) {}

String8::~String8() {}

} // namespace android
