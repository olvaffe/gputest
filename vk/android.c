/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

#include <android/choreographer.h>
#include <android/hardware_buffer.h>
#include <android/input.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/surface_control.h>
#include <threads.h>

enum android_test_looper_ident {
    MY_TEST_LOOPER_IDENT_INPUT,
};

struct android_test_state {
    AInputQueue *queue;
    ANativeWindow *win;
};

struct android_test {
    bool verbose;
    VkFormat vk_format;
    enum AHardwareBuffer_Format ahb_format;
    uint64_t ahb_usage;
    ANativeActivity *act;

    mtx_t mutex;
    cnd_t cond;
    thrd_t thread;
    bool run;

    ALooper *looper;
    AChoreographer *choreo;

    struct vk vk;

    struct android_test_state cur;
    struct android_test_state next;
    ASurfaceControl *ctrl;
};

static inline void
android_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    __android_log_vprint(ANDROID_LOG_INFO, "My", format, ap);
    va_end(ap);
}

static inline void
android_die(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    __android_log_vprint(ANDROID_LOG_INFO, "My", format, ap);
    va_end(ap);

    abort();
}

static void
android_test_ahb_draw_gpu(struct android_test *test, VkImage img)
{
    const bool protected = test->ahb_usage & AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT;
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, protected);
    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier barrier1 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
        .dstQueueFamilyIndex = vk->queue_family_index,
        .image = img,
        .subresourceRange = subres_range,
    };
    const VkImageMemoryBarrier barrier2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = vk->queue_family_index,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
        .image = img,
        .subresourceRange = subres_range,
    };

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0, 0, NULL, 0, NULL, 1, &barrier1);

    VkClearColorValue clear_val = {
        .float32 = { 0.5f, 0.5f, 0.5f, 1.0f },
    };
    clear_val.float32[protected ? 1 : 2] = 1.0f;

    vk->CmdClearColorImage(cmd, img, barrier1.newLayout, &clear_val, 1, &subres_range);

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1,
                           &barrier2);
    vk_end_cmd(vk);
    vk_wait(vk);
}

static VkDeviceMemory
android_test_ahb_create_memory(struct android_test *test, AHardwareBuffer *ahb, VkImage img)
{
    struct vk *vk = &test->vk;

    VkAndroidHardwareBufferPropertiesANDROID props = {
        .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,
    };
    vk->GetAndroidHardwareBufferPropertiesANDROID(vk->dev, ahb, &props);

    const uint32_t mt = ffs(props.memoryTypeBits) - 1;

    const VkImportAndroidHardwareBufferInfoANDROID import_info = {
        .sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
        .buffer = ahb,
    };
    const VkMemoryDedicatedAllocateInfo dedicated_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        .pNext = &import_info,
        .image = img,
    };
    const VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &dedicated_info,
        .allocationSize = props.allocationSize,
        .memoryTypeIndex = mt,
    };

    VkDeviceMemory mem;
    vk->result = vk->AllocateMemory(vk->dev, &alloc_info, NULL, &mem);
    vk_check(vk, "failed to import ahb");

    vk->result = vk->BindImageMemory(vk->dev, img, mem, 0);
    vk_check(vk, "failed to bind image memory");

    return mem;
}

static VkImage
android_test_ahb_create_image(struct android_test *test, AHardwareBuffer *ahb)
{
    struct vk *vk = &test->vk;

    AHardwareBuffer_Desc desc;
    AHardwareBuffer_describe(ahb, &desc);

    const VkImageCreateFlags img_flags = test->ahb_usage & AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT
                                             ? VK_IMAGE_CREATE_PROTECTED_BIT
                                             : 0;

    const VkPhysicalDeviceExternalImageFormatInfo fmt_ext_info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
    };
    const VkPhysicalDeviceImageFormatInfo2 fmt_info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
        .pNext = &fmt_ext_info,
        .format = test->vk_format,
        .type = VK_IMAGE_TYPE_2D,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .flags = img_flags,
    };
    VkExternalImageFormatProperties fmt_ext_props = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
    };
    VkImageFormatProperties2 fmt_props = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
        .pNext = &fmt_ext_props,
    };
    vk->result =
        vk->GetPhysicalDeviceImageFormatProperties2(vk->physical_dev, &fmt_info, &fmt_props);
    vk_check(vk, "unsupported image");

    const VkExternalMemoryFeatureFlags ext_mem_feats =
        fmt_ext_props.externalMemoryProperties.externalMemoryFeatures;
    if (!(ext_mem_feats & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT))
        vk_die("image does not support import");

    const VkExternalMemoryImageCreateInfo external_info = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
    };
    const VkImageCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &external_info,
        .flags = img_flags,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = test->vk_format,
        .extent = {
            .width = desc.width,
            .height = desc.height,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkImage img;
    vk->result = vk->CreateImage(vk->dev, &info, NULL, &img);
    vk_check(vk, "failed to create image");

    return img;
}

static void
android_test_ahb_draw_cpu(struct android_test *test, AHardwareBuffer *ahb)
{
    AHardwareBuffer_Desc desc;
    AHardwareBuffer_describe(ahb, &desc);

    void *ptr;
    int32_t cpp;
    int32_t stride;
    if (AHardwareBuffer_lockAndGetInfo(ahb, AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY, -1, NULL,
                                       &ptr, &cpp, &stride))
        android_die("failed to lock ahb");

    for (uint32_t y = 0; y < desc.height; y++) {
        void *row = ptr + stride * y;

        if (desc.format == AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM) {
            for (uint32_t x = 0; x < desc.width; x++) {
                uint8_t *rgba = row + cpp * x;
                rgba[0] = 0xff;
                rgba[1] = 0x80;
                rgba[2] = 0x80;
                rgba[3] = 0xff;
            }
        } else {
            memset(row, 0x80, cpp * desc.width);
        }
    }
}

static AHardwareBuffer *
android_test_ahb_alloc(struct android_test *test, uint32_t width, uint32_t height)
{
    const AHardwareBuffer_Desc desc = {
        .width = width,
        .height = height,
        .layers = 1,
        .format = test->ahb_format,
        .usage = test->ahb_usage,
    };

    AHardwareBuffer *ahb;
    if (AHardwareBuffer_allocate(&desc, &ahb))
        android_die("failed to alloc ahb");

    return ahb;
}

static void
android_test_handle_frame(int64_t ts, void *arg)
{
    struct android_test *test = arg;
    struct vk *vk = &test->vk;

    if (!test->ctrl)
        return;

    const uint32_t width = ANativeWindow_getWidth(test->cur.win);
    const uint32_t height = ANativeWindow_getHeight(test->cur.win);

    if (test->verbose) {
        android_log("frame: ts %" PRIi64 ", %dx%d, format 0x%x, usage 0x%" PRIx64, ts, width,
                    height, test->ahb_format, test->ahb_usage);
    }

    AHardwareBuffer *ahb = android_test_ahb_alloc(test, width, height);

    if (test->ahb_usage & AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER) {
        VkImage img = android_test_ahb_create_image(test, ahb);
        VkDeviceMemory mem = android_test_ahb_create_memory(test, ahb, img);

        android_test_ahb_draw_gpu(test, img);

        vk->FreeMemory(vk->dev, mem, NULL);
        vk->DestroyImage(vk->dev, img, NULL);
    } else if ((test->ahb_usage & AHARDWAREBUFFER_USAGE_CPU_WRITE_MASK) !=
               AHARDWAREBUFFER_USAGE_CPU_WRITE_NEVER) {
        android_test_ahb_draw_cpu(test, ahb);
    }

    ASurfaceTransaction *xact = ASurfaceTransaction_create();
    ASurfaceTransaction_setBuffer(xact, test->ctrl, ahb, -1);
    ASurfaceTransaction_apply(xact);
    ASurfaceTransaction_delete(xact);

    AHardwareBuffer_release(ahb);
}

static void
android_test_handle_input(struct android_test *test)
{
    AInputQueue *queue = test->cur.queue;
    AInputEvent *ev;

    while (AInputQueue_getEvent(queue, &ev) >= 0) {
        if (!AInputQueue_preDispatchEvent(queue, ev))
            AInputQueue_finishEvent(queue, ev, false);
    }
}

static void
android_test_handle_state(struct android_test *test)
{
    struct android_test_state *cur = &test->cur;
    struct android_test_state *next = &test->next;

    if (!memcmp(cur, next, sizeof(*cur)))
        return;

    if (cur->queue != next->queue) {
        if (cur->queue)
            AInputQueue_detachLooper(cur->queue);

        cur->queue = next->queue;

        if (cur->queue) {
            AInputQueue_attachLooper(cur->queue, test->looper, MY_TEST_LOOPER_IDENT_INPUT, NULL,
                                     NULL);
        }
    }

    if (cur->win != next->win) {
        if (cur->win) {
            ASurfaceControl_release(test->ctrl);
            test->ctrl = NULL;
        }

        cur->win = next->win;

        if (cur->win) {
            test->ctrl = ASurfaceControl_createFromWindow(cur->win, "MySurfaceControl");
            if (!test->ctrl)
                android_die("failed to create surface control");

            AChoreographer_postFrameCallback64(test->choreo, android_test_handle_frame, test);
        }
    }

    cnd_signal(&test->cond);
}

static int
android_test_thread(void *arg)
{
    struct android_test *test = arg;
    struct vk *vk = &test->vk;

    mtx_lock(&test->mutex);

    test->looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    if (!test->looper)
        android_die("failed to prepare looper");

    test->choreo = AChoreographer_getInstance();
    if (!test->choreo)
        android_die("failed to get choreographer");

    const char *const dev_exts[] = {
        VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME,
        VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
    };
    const struct vk_init_params vk_params = {
        .protected_memory = test->ahb_usage & AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT,
        .dev_exts = dev_exts,
        .dev_ext_count = ARRAY_SIZE(dev_exts),
    };
    vk_init(vk, &vk_params);

    /* signal readiness */
    test->run = true;
    cnd_signal(&test->cond);
    android_log("thread ready");

    while (test->run) {
        mtx_unlock(&test->mutex);
        const int ident = ALooper_pollOnce(-1, NULL, NULL, NULL);
        mtx_lock(&test->mutex);

        switch (ident) {
        case ALOOPER_POLL_ERROR:
            android_die("failed to poll");
            break;
        case MY_TEST_LOOPER_IDENT_INPUT:
            android_test_handle_input(test);
            break;
        default:
            break;
        }

        android_test_handle_state(test);
    }

    vk_cleanup(vk);

    if (test->cur.queue)
        AInputQueue_detachLooper(test->cur.queue);
    test->looper = NULL;
    test->choreo = NULL;

    mtx_unlock(&test->mutex);

    return 0;
}

static void
android_test_init(struct android_test *test)
{
    if (mtx_init(&test->mutex, mtx_plain) != thrd_success)
        android_die("failed to init mutex");

    if (cnd_init(&test->cond) != thrd_success)
        android_die("failed to init cond");

    if (thrd_create(&test->thread, android_test_thread, test) != thrd_success)
        android_die("failed to create thread");

    /* wait for readiness */
    mtx_lock(&test->mutex);
    while (!test->run)
        cnd_wait(&test->cond, &test->mutex);
    mtx_unlock(&test->mutex);

    android_log("main ready");
}

static void
android_test_cleanup(struct android_test *test)
{
    mtx_lock(&test->mutex);
    test->run = false;
    ALooper_wake(test->looper);
    mtx_unlock(&test->mutex);

    thrd_join(test->thread, NULL);

    cnd_destroy(&test->cond);
    mtx_destroy(&test->mutex);
}

static void
android_test_set_window(struct android_test *test, ANativeWindow *win)
{
    mtx_lock(&test->mutex);

    test->next.win = win;
    ALooper_wake(test->looper);

    if (!win) {
        while (test->cur.win != test->next.win)
            cnd_wait(&test->cond, &test->mutex);
    }

    mtx_unlock(&test->mutex);
}

static void
android_test_set_queue(struct android_test *test, AInputQueue *queue)
{
    mtx_lock(&test->mutex);

    test->next.queue = queue;
    ALooper_wake(test->looper);

    if (!queue) {
        while (test->cur.queue != test->next.queue)
            cnd_wait(&test->cond, &test->mutex);
    }

    mtx_unlock(&test->mutex);
}

static void
ANativeActivity_onStart(ANativeActivity *act)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onStart");
}

static void
ANativeActivity_onResume(ANativeActivity *act)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onResume");
}

static void *
ANativeActivity_onSaveInstanceState(ANativeActivity *act, size_t *outSize)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onSaveInstanceState");

    *outSize = 0;
    return NULL;
}

static void
ANativeActivity_onPause(ANativeActivity *act)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onPause");
}

static void
ANativeActivity_onStop(ANativeActivity *act)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onStop");
}

static void
ANativeActivity_onDestroy(ANativeActivity *act)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onDestroy");

    android_test_cleanup(test);

    free(test);
    act->instance = NULL;
}

static void
ANativeActivity_onWindowFocusChanged(ANativeActivity *act, int hasFocus)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onWindowFocusChanged: %d", hasFocus);
}

static void
ANativeActivity_onNativeWindowCreated(ANativeActivity *act, ANativeWindow *win)
{
    struct android_test *test = act->instance;

    if (test->verbose) {
        android_log("onNativeWindowCreated: %p, %dx%d, format 0x%x", win,
                    ANativeWindow_getWidth(win), ANativeWindow_getHeight(win),
                    ANativeWindow_getFormat(win));
    }

    android_test_set_window(test, win);
}

static void
ANativeActivity_onNativeWindowResized(ANativeActivity *act, ANativeWindow *win)
{
    struct android_test *test = act->instance;

    if (test->verbose) {
        android_log("onNativeWindowResized: %p, %dx%d", win, ANativeWindow_getWidth(win),
                    ANativeWindow_getHeight(win));
    }
}

static void
ANativeActivity_onNativeWindowRedrawNeeded(ANativeActivity *act, ANativeWindow *win)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onNativeWindowRedrawNeeded: %p", win);

    mtx_lock(&test->mutex);
    AChoreographer_postFrameCallback64(test->choreo, android_test_handle_frame, test);
    mtx_unlock(&test->mutex);
}

static void
ANativeActivity_onNativeWindowDestroyed(ANativeActivity *act, ANativeWindow *win)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onNativeWindowDestroyed: %p", win);

    android_test_set_window(test, NULL);
}

static void
ANativeActivity_onInputQueueCreated(ANativeActivity *act, AInputQueue *queue)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onInputQueueCreated: %p", queue);

    android_test_set_queue(test, queue);
}

static void
ANativeActivity_onInputQueueDestroyed(ANativeActivity *act, AInputQueue *queue)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onInputQueueDestroyed: %p", queue);

    android_test_set_queue(test, NULL);
}

static void
ANativeActivity_onContentRectChanged(ANativeActivity *act, const ARect *rect)
{
    struct android_test *test = act->instance;

    if (test->verbose) {
        android_log("onContentRectChanged: (%d, %d, %d, %d)", rect->left, rect->top, rect->right,
                    rect->bottom);
    }
}

static void
ANativeActivity_onConfigurationChanged(ANativeActivity *act)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onConfigurationChanged");
}

static void
ANativeActivity_onLowMemory(ANativeActivity *act)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onLowMemory");
}

void
ANativeActivity_onCreate(ANativeActivity *act, void *savedState, size_t savedStateSize)
{
    const struct android_test test_templ = {
        .verbose = true,
        .vk_format = VK_FORMAT_R8G8B8A8_UNORM,
        .ahb_format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
        .ahb_usage = AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER |
                     AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                     AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY,
        .act = act,
    };

    struct android_test *test = calloc(1, sizeof(*test));
    if (!test)
        android_die("failed to alloc test");
    *test = test_templ;

    if (test->verbose) {
        android_log("onCreate: sdk %d, internal %s, external %s", act->sdkVersion,
                    act->internalDataPath, act->externalDataPath);
    }

    if (!(test->ahb_usage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE) ||
        !(test->ahb_usage & AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY))
        android_die("missing sf usage");

    if (test->ahb_usage & AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT) {
        if ((test->ahb_usage & AHARDWAREBUFFER_USAGE_CPU_READ_MASK) !=
            AHARDWAREBUFFER_USAGE_CPU_READ_NEVER)
            android_die("protected with cpu read");
        if ((test->ahb_usage & AHARDWAREBUFFER_USAGE_CPU_WRITE_MASK) !=
            AHARDWAREBUFFER_USAGE_CPU_WRITE_NEVER)
            android_die("protected with cpu write");
        if (!(test->ahb_usage & AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER))
            android_die("protected without gpu fb");
    } else {
        if (!(test->ahb_usage & AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER) &&
            (test->ahb_usage & AHARDWAREBUFFER_USAGE_CPU_WRITE_MASK) ==
                AHARDWAREBUFFER_USAGE_CPU_WRITE_NEVER)
            android_die("no cpu or gpu write");
    }

    ANativeActivityCallbacks *cbs = act->callbacks;
    cbs->onStart = ANativeActivity_onStart;
    cbs->onResume = ANativeActivity_onResume;
    cbs->onSaveInstanceState = ANativeActivity_onSaveInstanceState;
    cbs->onPause = ANativeActivity_onPause;
    cbs->onStop = ANativeActivity_onStop;
    cbs->onDestroy = ANativeActivity_onDestroy;
    cbs->onWindowFocusChanged = ANativeActivity_onWindowFocusChanged;
    cbs->onNativeWindowCreated = ANativeActivity_onNativeWindowCreated;
    cbs->onNativeWindowResized = ANativeActivity_onNativeWindowResized;
    cbs->onNativeWindowRedrawNeeded = ANativeActivity_onNativeWindowRedrawNeeded;
    cbs->onNativeWindowDestroyed = ANativeActivity_onNativeWindowDestroyed;
    cbs->onInputQueueCreated = ANativeActivity_onInputQueueCreated;
    cbs->onInputQueueDestroyed = ANativeActivity_onInputQueueDestroyed;
    cbs->onContentRectChanged = ANativeActivity_onContentRectChanged;
    cbs->onConfigurationChanged = ANativeActivity_onConfigurationChanged;
    cbs->onLowMemory = ANativeActivity_onLowMemory;

    act->instance = test;

    android_test_init(test);
}
