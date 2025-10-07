/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "dmautil.h"
#include "eglutil.h"

struct fence_test {
    struct egl egl;
};

static void
fence_test_init(struct fence_test *test)
{
    struct egl *egl = &test->egl;

    egl_init(egl, NULL);

    if (!strstr(egl->dpy_exts, "EGL_ANDROID_native_fence_sync"))
        egl_die("no EGL_ANDROID_native_fence_sync");

    egl_check(egl, "init");
}

static void
fence_test_cleanup(struct fence_test *test)
{
    struct egl *egl = &test->egl;

    egl_check(egl, "cleanup");

    egl_cleanup(egl);
}

static void
fence_test_dump(struct fence_test *test, int fd)
{
    struct sync_file_info *info = dma_sync_file_info(fd);

    egl_log("name: %s", info->name);
    egl_log("status: %d", info->status);
    egl_log("flags: 0x%x", info->flags);

    for (uint32_t i = 0; i < info->num_fences; i++) {
        const struct sync_fence_info *fences = (void *)(uintptr_t)info->sync_fence_info;
        const struct sync_fence_info *fence = &fences[i];

        egl_log("fences[%d]", i);
        egl_log("  obj_name: %s", fence->obj_name);
        egl_log("  driver_name: %s", fence->driver_name);
        egl_log("  status: %d", fence->status);
        egl_log("  flags: 0x%x", fence->flags);
        egl_log("  timestamp_ns: %" PRIu64 " (ktime)", (uint64_t)fence->timestamp_ns);
    }

    free(info);
}

static void
fence_test_draw(struct fence_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    const uint64_t begin = u_now();

    EGLSync sync = egl->CreateSync(egl->dpy, EGL_SYNC_NATIVE_FENCE_ANDROID, NULL);
    egl_check(egl, "sync");

    int fd = egl->DupNativeFenceFDANDROID(egl->dpy, sync);
    if (fd < 0) {
        egl_log("glFlush");
        gl->Flush();
        fd = egl->DupNativeFenceFDANDROID(egl->dpy, sync);
    }
    egl_check(egl, "dup");

    if (fd < 0)
        egl_die("failed to dup");

    egl_log("begin: %" PRIu64 " (CLOCK_MONOTONIC)", begin);
    fence_test_dump(test, fd);

    egl_log("glFinish");
    gl->Finish();
    fence_test_dump(test, fd);

    egl->DestroySync(egl->dpy, sync);
    close(fd);
}

int
main(int argc, const char **argv)
{
    struct fence_test test = { 0 };

    fence_test_init(&test);
    fence_test_draw(&test);
    fence_test_cleanup(&test);

    return 0;
}
