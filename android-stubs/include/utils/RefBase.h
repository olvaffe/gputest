#pragma once

namespace android {

// sizeof(stub) == sizeof(real). The vtable is truncated. All non-virtual
// methods are defined in libutils.
class RefBase {
  public:
    void incStrong(const void *id) const;
    void decStrong(const void *id) const;

  protected:
    RefBase();
    virtual ~RefBase();

  private:
    [[maybe_unused]] void *mRefs;
};

} // namespace android
