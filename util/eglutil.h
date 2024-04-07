/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef EGLUTIL_H
#define EGLUTIL_H

#define EGL_EGL_PROTOTYPES 0
#define GL_GLES_PROTOTYPES 0

#include "util.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
/* clang-format off */
#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>
/* clang-format on */
#include <dlfcn.h>
#include <fcntl.h>
#include <strings.h>
#include <unistd.h>

#ifdef __ANDROID__

#include <android/hardware_buffer.h>

#define LIBEGL_NAME "libEGL.so"

struct gbm_device;

#else /* __ANDROID__ */

#include <gbm.h>

#define LIBEGL_NAME "libEGL.so.1"

#endif /* __ANDROID__ */

#define PRINTFLIKE(f, a) __attribute__((format(printf, f, a)))
#define NORETURN __attribute__((noreturn))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

struct egl_gl {
#define PFN_GL(proc, name) PFNGL##proc##PROC name;
#include "eglutil_entrypoints.inc"
};

struct egl_drm_format {
    int drm_format;
    int drm_modifier_count;
    const EGLuint64KHR *drm_modifiers;
    const EGLBoolean *external_only;
};

struct egl_init_params {
    EGLint pbuffer_width;
    EGLint pbuffer_height;
};

struct egl {
    struct egl_init_params params;

    struct {
        void *handle;

#define PFN_EGL(proc, name) PFNEGL##proc##PROC name;
#include "eglutil_entrypoints.inc"
        struct egl_gl gl;

        const char *client_exts;
    };

    EGLDeviceEXT dev;
    EGLDisplay dpy;
    EGLint major;
    EGLint minor;

    const char *dpy_exts;
    bool KHR_no_config_context;
    bool EXT_image_dma_buf_import;
    bool EXT_image_dma_buf_import_modifiers;
    bool ANDROID_get_native_client_buffer;
    bool ANDROID_image_native_buffer;

    struct gbm_device *gbm;
    int gbm_fd;
    bool is_minigbm;

    EGLConfig config;
    EGLSurface surf;

    EGLContext ctx;

    int drm_format_count;
    struct egl_drm_format **drm_formats;

    const char *gl_exts;
};

struct egl_framebuffer {
    GLuint fbo;
    GLuint tex;
};

struct egl_program {
    GLuint vs;
    GLuint fs;
    GLuint prog;
};

struct egl_image_info {
    EGLContext ctx;
    EGLenum target;
    EGLClientBuffer buf;

    int dma_buf_fd;
    int width;
    int height;
    int drm_format;
    uint64_t drm_modifier;
    int mem_plane_count;
    int offsets[4];
    int pitches[4];
};

struct egl_image_storage_info {
    int width;
    int height;
    int drm_format;

    bool mapping;
    bool rendering;
    bool sampling;
    bool force_linear;
};

struct egl_image_storage {
    void *obj;
    struct egl_image_info info;

    void *planes[3];
    int strides[3];
    void *bo_xfer;
};

struct egl_image {
    struct egl_image_storage *storage;

    EGLImage img;
};

static inline void PRINTFLIKE(1, 2) egl_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_logv("EGL", format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(1, 2) NORETURN egl_die(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_diev("EGL", format, ap);
    va_end(ap);
}

static inline void
egl_check(struct egl *egl, const char *where)
{
    const EGLint egl_err = egl->GetError();
    if (egl_err != EGL_SUCCESS)
        egl_die("%s: egl has error 0x%04x", where, egl_err);

    if (egl->ctx) {
        const GLenum gl_err = egl->gl.GetError();
        if (gl_err != GL_NO_ERROR)
            egl_die("%s: gl has error 0x%04x", where, gl_err);
    }
}

#ifdef __ANDROID__

static inline void
egl_init_image_allocator(struct egl *egl)
{
    egl->gbm_fd = -1;
}

static inline void
egl_cleanup_image_allocator(struct egl *egl)
{
}

static inline enum AHardwareBuffer_Format
egl_drm_format_to_ahb_format(int drm_format)
{
    /* sanity check */
    u_drm_format_to_cpp(drm_format);

    switch (drm_format) {
    case DRM_FORMAT_ABGR8888:
        return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    case DRM_FORMAT_XBGR8888:
        return AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM;
    case DRM_FORMAT_BGR888:
        return AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM;
    case DRM_FORMAT_RGB565:
        return AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
    case DRM_FORMAT_ABGR16161616F:
        return AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
    case DRM_FORMAT_ABGR2101010:
        return AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
    case DRM_FORMAT_R8:
        return AHARDWAREBUFFER_FORMAT_BLOB;
#if __ANDROID_API__ >= 29
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_YVU420:
        /* there is no guarantee gralloc would pick NV12 or YVU420.. */
        return AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420;
    case DRM_FORMAT_P010:
        return AHARDWAREBUFFER_FORMAT_YCbCr_P010;
#endif
    default:
        egl_die("unsupported drm format 0x%x", drm_format);
    }
}

static inline struct egl_image_storage *
egl_alloc_image_storage(struct egl *egl, const struct egl_image_storage_info *info)
{
    if (info->force_linear)
        egl_log("cannot force linear in AHB");

    const enum AHardwareBuffer_Format format = egl_drm_format_to_ahb_format(info->drm_format);
    uint64_t usage = 0;
    if (info->mapping)
        usage |= AHARDWAREBUFFER_USAGE_CPU_READ_RARELY | AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY;
    if (info->rendering)
        usage |= AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER;
    if (info->sampling)
        usage |= AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;

    const AHardwareBuffer_Desc desc = {
        .width = info->width,
        .height = info->height,
        .layers = 1,
        .format = format,
        .usage = usage,
    };
    AHardwareBuffer *ahb;
    if (AHardwareBuffer_allocate(&desc, &ahb))
        egl_die("failed to create ahb");

    if (!egl->ANDROID_get_native_client_buffer)
        egl_die("no ahb import support");
    EGLClientBuffer buf = egl->GetNativeClientBufferANDROID(ahb);
    if (!buf)
        egl_die("failed to get client buffer from ahb");

    struct egl_image_storage *storage = calloc(1, sizeof(*storage));
    if (!storage)
        egl_die("failed to alloc storage");

    storage->obj = ahb;
    storage->info.ctx = EGL_NO_CONTEXT;
    storage->info.target = EGL_NATIVE_BUFFER_ANDROID;
    storage->info.buf = buf;
    storage->info.width = info->width;
    storage->info.height = info->height;
    storage->info.drm_format = info->drm_format;

    return storage;
}

static inline void
egl_free_image_storage(struct egl *egl, struct egl_image_storage *storage)
{
    AHardwareBuffer_release(storage->obj);
    free(storage);
}

static inline void
egl_map_image_storage(struct egl *egl, struct egl_image_storage *storage)
{
    const uint64_t usage =
        AHARDWAREBUFFER_USAGE_CPU_READ_RARELY | AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY;
    const ARect rect = { .right = storage->info.width, .bottom = storage->info.height };
    AHardwareBuffer *ahb = storage->obj;

#if __ANDROID_API__ >= 29
    AHardwareBuffer_Planes planes;
    if (AHardwareBuffer_lockPlanes(ahb, usage, -1, &rect, &planes))
        egl_die("failed to lock ahb");

    const int plane_count = u_drm_format_to_plane_count(storage->info.drm_format);
    for (int i = 0; i < plane_count; i++) {
        storage->planes[i] = planes.planes[i].data;
        storage->strides[i] = planes.planes[i].rowStride;
    }

    if (plane_count < planes.planeCount) {
        if (plane_count != 2 || planes.planeCount != 3 ||
            planes.planes[1].rowStride != planes.planes[2].rowStride ||
            planes.planes[1].pixelStride != 2 || planes.planes[2].pixelStride != 2 ||
            planes.planes[1].data + 1 != planes.planes[2].data)
            egl_die("ahb cb/cr is not interleaved");
    }
#else
    if (u_drm_format_to_plane_count(storage->info.drm_format) > 1)
        egl_die("no AHardwareBuffer_lockPlanes support");

    void *ptr;
    if (AHardwareBuffer_lock(ahb, usage, -1, &rect, &ptr))
        egl_die("failed to lock ahb");

    AHardwareBuffer_Desc desc;
    AHardwareBuffer_describe(ahb, &desc);

    const int cpp = u_drm_format_to_cpp(storage->info.drm_format);
    storage->planes[0] = ptr;
    storage->strides[0] = desc.stride * cpp;
#endif

    storage->bo_xfer = NULL;
}

static inline void
egl_unmap_image_storage(struct egl *egl, struct egl_image_storage *storage)
{
    AHardwareBuffer_unlock(storage->obj, NULL);
}

#else /* __ANDROID__ */

static inline void
egl_init_image_allocator(struct egl *egl)
{
    if (egl->dev == EGL_NO_DEVICE_EXT)
        egl_die("gbm requires EGLDeviceEXT");

    egl->gbm_fd = -1;

    const char *node = egl->QueryDeviceStringEXT(egl->dev, EGL_DRM_RENDER_NODE_FILE_EXT);
    if (!node)
        return;

    egl->gbm_fd = open(node, O_RDWR | O_CLOEXEC);
    if (egl->gbm_fd < 0)
        egl_die("failed to open %s", node);

    egl->gbm = gbm_create_device(egl->gbm_fd);
    if (!egl->gbm)
        egl_die("failed to create gbm device");

    const char *gbm_name = gbm_device_get_backend_name(egl->gbm);
    if (strcmp(gbm_name, "drm")) {
        egl_log("detected minigbm");
        egl->is_minigbm = true;
    }
}

static inline void
egl_cleanup_image_allocator(struct egl *egl)
{
    if (egl->gbm) {
        gbm_device_destroy(egl->gbm);
        close(egl->gbm_fd);
    }
}

static inline const struct egl_drm_format *
egl_find_drm_format(const struct egl *egl, int drm_format)
{
    for (int i = 0; i < egl->drm_format_count; i++) {
        if (egl->drm_formats[i]->drm_format == drm_format)
            return egl->drm_formats[i];
    }
    return NULL;
}

static inline const uint64_t *
egl_find_drm_modifier(const struct egl_drm_format *fmt, uint64_t drm_modifier)
{
    for (int i = 0; i < fmt->drm_modifier_count; i++) {
        if (fmt->drm_modifiers[i] == drm_modifier) {
            return &fmt->drm_modifiers[i];
        }
    }
    return NULL;
}

static inline struct egl_image_storage *
egl_alloc_image_storage(struct egl *egl, const struct egl_image_storage_info *info)
{
    const struct egl_drm_format *fmt = egl_find_drm_format(egl, info->drm_format);
    if (!fmt)
        egl_die("unsupported drm format 0x%08x", info->drm_format);

    const uint64_t *drm_modifiers = fmt->drm_modifiers;
    int drm_modifier_count = fmt->drm_modifier_count;
    if (info->force_linear) {
        drm_modifiers = egl_find_drm_modifier(fmt, DRM_FORMAT_MOD_LINEAR);
        if (!drm_modifiers)
            egl_die("failed to find linear modifier");
        drm_modifier_count = 1;
    }

    struct gbm_bo *bo = gbm_bo_create_with_modifiers(
        egl->gbm, info->width, info->height, info->drm_format, drm_modifiers, drm_modifier_count);
    if (!bo)
        egl_die("failed to create gbm bo");

    const int plane_count = gbm_bo_get_plane_count(bo);
    if (plane_count > 1) {
        /* make sure all planes have the same handle (for mapping) */
        const union gbm_bo_handle handle = gbm_bo_get_handle_for_plane(bo, 0);
        for (int i = 1; i < plane_count; i++) {
            const union gbm_bo_handle h = gbm_bo_get_handle_for_plane(bo, i);
            if (memcmp(&handle, &h, sizeof(h)))
                egl_die("bo planes have different handles");
        }
    }

    struct egl_image_storage *storage = calloc(1, sizeof(*storage));
    if (!storage)
        egl_die("failed to alloc storage");

    storage->obj = bo;
    storage->info.ctx = EGL_NO_CONTEXT;
    storage->info.target = EGL_LINUX_DMA_BUF_EXT;
    storage->info.buf = NULL;

    storage->info.width = info->width;
    storage->info.height = info->height;
    storage->info.drm_format = info->drm_format;
    storage->info.drm_modifier = gbm_bo_get_modifier(bo);

    storage->info.mem_plane_count = gbm_bo_get_plane_count(bo);
    if (storage->info.mem_plane_count > 4)
        egl_die("unexpected plane count");
    for (int i = 0; i < storage->info.mem_plane_count; i++) {
        storage->info.offsets[i] = gbm_bo_get_offset(bo, i);
        storage->info.pitches[i] = gbm_bo_get_stride_for_plane(bo, i);
    }

    storage->info.dma_buf_fd = gbm_bo_get_fd_for_plane(bo, 0);
    if (storage->info.dma_buf_fd < 0)
        egl_die("failed to export gbm bo");

    return storage;
}

static inline void
egl_free_image_storage(struct egl *egl, struct egl_image_storage *storage)
{
    close(storage->info.dma_buf_fd);
    gbm_bo_destroy(storage->obj);
    free(storage);
}

static inline void
egl_map_image_storage(struct egl *egl, struct egl_image_storage *storage)
{
    struct gbm_bo *bo = storage->obj;

    uint32_t stride;
    void *xfer = NULL;
    void *ptr = gbm_bo_map(bo, 0, 0, storage->info.width, storage->info.height,
                           GBM_BO_TRANSFER_READ_WRITE, &stride, &xfer);
    if (!ptr)
        egl_die("failed to map bo");

    const int plane_count = u_drm_format_to_plane_count(storage->info.drm_format);
    if (plane_count > 1) {
        if (plane_count > gbm_bo_get_plane_count(bo))
            egl_die("unexpected bo plane count");

        for (int i = 0; i < plane_count; i++) {
            storage->planes[i] = ptr + gbm_bo_get_offset(bo, i);
            storage->strides[i] = gbm_bo_get_stride_for_plane(bo, i);
        }
    } else {
        storage->planes[0] = ptr;
        storage->strides[0] = stride;
    }

    storage->bo_xfer = xfer;
}

static inline void
egl_unmap_image_storage(struct egl *egl, struct egl_image_storage *storage)
{
    gbm_bo_unmap(storage->obj, storage->bo_xfer);
}

#endif /* __ANDROID__ */

static inline void
egl_init_library_dispatch(struct egl *egl)
{
    /* we assume EGL 1.5, which includes EGL_EXT_client_extensions and
     * EGL_KHR_client_get_all_proc_addresses
     */
#define PFN_EGL_EXT(proc, name) egl->name = (PFNEGL##proc##PROC)egl->GetProcAddress("egl" #name);
#define PFN_GL_EXT(proc, name) egl->gl.name = (PFNGL##proc##PROC)egl->GetProcAddress("gl" #name);
#define PFN_EGL(proc, name)                                                                      \
    PFN_EGL_EXT(proc, name)                                                                      \
    if (!egl->name)                                                                              \
        egl_die("no egl" #name);
#define PFN_GL(proc, name)                                                                       \
    PFN_GL_EXT(proc, name)                                                                       \
    if (!egl->gl.name)                                                                           \
        egl_die("no gl" #name);
#include "eglutil_entrypoints.inc"
}

static inline void
egl_init_library(struct egl *egl)
{
    egl->handle = dlopen(LIBEGL_NAME, RTLD_LOCAL | RTLD_LAZY);
    if (!egl->handle)
        egl_die("failed to load %s: %s", LIBEGL_NAME, dlerror());

    const char gipa_name[] = "eglGetProcAddress";
    egl->GetProcAddress = dlsym(egl->handle, gipa_name);
    if (!egl->GetProcAddress)
        egl_die("failed to find %s: %s", gipa_name, dlerror());

    egl_init_library_dispatch(egl);

    egl->client_exts = egl->QueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (!egl->client_exts) {
#ifdef __ANDROID__
        egl_log("no client extension");
        egl->client_exts = "";
        egl->GetError();
#else
        egl_die("no client extension");
#endif
    }
}

static inline void
egl_init_display_extensions(struct egl *egl)
{
    egl->dpy_exts = egl->QueryString(egl->dpy, EGL_EXTENSIONS);

    egl->KHR_no_config_context = strstr(egl->dpy_exts, "EGL_KHR_no_config_context");
    egl->EXT_image_dma_buf_import = strstr(egl->dpy_exts, "EGL_EXT_image_dma_buf_import");
    egl->EXT_image_dma_buf_import_modifiers =
        strstr(egl->dpy_exts, "EGL_EXT_image_dma_buf_import_modifiers");
    egl->ANDROID_get_native_client_buffer =
        strstr(egl->dpy_exts, "EGL_ANDROID_get_native_client_buffer");
    egl->ANDROID_image_native_buffer = strstr(egl->dpy_exts, "EGL_ANDROID_image_native_buffer");
}

static inline void
egl_init_display(struct egl *egl)
{
    const bool EXT_device_enumeration = strstr(egl->client_exts, "EGL_EXT_device_enumeration");
    const bool EXT_device_query = strstr(egl->client_exts, "EGL_EXT_device_query");
    const bool EXT_platform_device = strstr(egl->client_exts, "EGL_EXT_platform_device");
    const bool KHR_platform_android = strstr(egl->client_exts, "EGL_KHR_platform_android");

    if (EXT_device_enumeration && EXT_device_query && EXT_platform_device) {
        egl_log("using platform device");

        EGLDeviceEXT devs[16];
        EGLint count;
        if (!egl->QueryDevicesEXT(ARRAY_SIZE(devs), devs, &count))
            egl_die("failed to query devices");

        egl->dev = EGL_NO_DEVICE_EXT;
        for (int i = 0; i < count; i++) {
            const char *exts = egl->QueryDeviceStringEXT(devs[i], EGL_EXTENSIONS);

            /* EGL_EXT_device_drm_render_node */
            if (!strstr(exts, "EGL_EXT_device_drm_render_node"))
                continue;

            const bool swrast = strstr(exts, "software");
            if (!swrast) {
                egl->dev = devs[i];
                break;
            }
        }
        if (egl->dev == EGL_NO_DEVICE_EXT)
            egl_die("failed to find a hw rendernode device");

        egl->dpy = egl->GetPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, egl->dev, NULL);
    } else if (KHR_platform_android) {
        egl_log("using platform android");

        egl->dev = EGL_NO_DEVICE_EXT;
        egl->dpy = egl->GetPlatformDisplay(EGL_PLATFORM_ANDROID_KHR, EGL_DEFAULT_DISPLAY, NULL);
    } else {
        egl_log("using EGL_DEFAULT_DISPLAY");
        egl->dev = EGL_NO_DEVICE_EXT;
        egl->dpy = egl->GetDisplay(EGL_DEFAULT_DISPLAY);
    }

    if (egl->dpy == EGL_NO_DISPLAY)
        egl_die("failed to get platform display");

    if (!egl->Initialize(egl->dpy, &egl->major, &egl->minor))
        egl_die("failed to initialize display");

    egl_init_display_extensions(egl);

    if (egl->major != 1 || egl->minor < 5) {
#ifdef __ANDROID__
        egl_log("fixing up for EGL %d.%d", egl->major, egl->minor);
        if (!strstr(egl->dpy_exts, "EGL_KHR_image_base"))
            egl_die("no EGL_KHR_image_base");
        egl->CreateImage = (PFNEGLCREATEIMAGEPROC)egl->GetProcAddress("eglCreateImageKHR");
        egl->DestroyImage = (PFNEGLDESTROYIMAGEPROC)egl->GetProcAddress("eglDestroyImageKHR");
#else
        egl_die("EGL 1.5 is required");
#endif
    }
}

static inline void
egl_init_config_and_surface(struct egl *egl)
{
    const bool with_pbuffer = egl->params.pbuffer_width && egl->params.pbuffer_height;
    if (egl->KHR_no_config_context && !with_pbuffer) {
        egl_log("using EGL_NO_CONFIG_KHR");
        egl->config = EGL_NO_CONFIG_KHR;
        return;
    }

    const EGLint config_attrs[] = {
        EGL_RED_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_BLUE_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8,
        EGL_RENDERABLE_TYPE,
        EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,
        with_pbuffer ? EGL_PBUFFER_BIT : 0,
        EGL_NONE,
    };

    EGLint count;
    if (!egl->ChooseConfig(egl->dpy, config_attrs, &egl->config, 1, &count) || !count)
        egl_die("failed to choose a config");

    if (!with_pbuffer) {
        egl_log("using EGL_NO_SURFACE");
        egl->surf = EGL_NO_SURFACE;
        return;
    }

    const EGLint surf_attrs[] = {
        EGL_WIDTH, egl->params.pbuffer_width, EGL_HEIGHT, egl->params.pbuffer_height, EGL_NONE,
    };

    egl->surf = egl->CreatePbufferSurface(egl->dpy, egl->config, surf_attrs);
    if (egl->surf == EGL_NO_SURFACE)
        egl_die("failed to create pbuffer surface");
}

static inline void
egl_init_context(struct egl *egl)
{
    if (egl->QueryAPI() != EGL_OPENGL_ES_API)
        egl_die("current api is not GLES");

    const EGLint ctx_attrs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE,
    };

    EGLContext ctx = egl->CreateContext(egl->dpy, egl->config, EGL_NO_CONTEXT, ctx_attrs);
    if (ctx == EGL_NO_CONTEXT)
        egl_die("failed to create a context");

    if (!egl->MakeCurrent(egl->dpy, egl->surf, egl->surf, ctx))
        egl_die("failed to make context current");

    egl->ctx = ctx;
}

static inline void
egl_init_drm_formats(struct egl *egl)
{
    if (!egl->EXT_image_dma_buf_import_modifiers)
        return;

    EGLint fmt_count;
    if (!egl->QueryDmaBufFormatsEXT(egl->dpy, 0, NULL, &fmt_count))
        egl_die("failed to get dma-buf format count");

    struct egl_drm_format **fmts = malloc(sizeof(*fmts) * fmt_count);
    EGLint *drm_fmts = malloc(sizeof(*drm_fmts) * fmt_count);
    if (!fmts || !drm_fmts)
        egl_die("failed to alloc fmts");

    if (!egl->QueryDmaBufFormatsEXT(egl->dpy, fmt_count, drm_fmts, &fmt_count))
        egl_die("failed to get dma-buf formats");

    for (int i = 0; i < fmt_count; i++) {
        const EGLint drm_fmt = drm_fmts[i];

        EGLint mod_count;
        if (!egl->QueryDmaBufModifiersEXT(egl->dpy, drm_fmt, 0, NULL, NULL, &mod_count))
            egl_die("failed to get dma-buf modifier count");

        struct egl_drm_format *fmt =
            malloc(sizeof(*fmt) + sizeof(fmt->drm_modifiers) * mod_count +
                   sizeof(fmt->external_only) * mod_count);
        if (!fmt)
            egl_die("failed to alloc fmt");
        EGLuint64KHR *drm_modifiers = (EGLuint64KHR *)(fmt + 1);
        EGLBoolean *external_only = (EGLBoolean *)(drm_modifiers + mod_count);

        if (!egl->QueryDmaBufModifiersEXT(egl->dpy, drm_fmt, mod_count, drm_modifiers,
                                          external_only, &mod_count))
            egl_die("failed to get dma-buf modifiers");

        fmt->drm_format = drm_fmt;
        fmt->drm_modifier_count = mod_count;
        fmt->drm_modifiers = drm_modifiers;
        fmt->external_only = external_only;
        fmts[i] = fmt;
    }

    free(drm_fmts);

    egl->drm_format_count = fmt_count;
    egl->drm_formats = fmts;
}

static inline void
egl_init_gl(struct egl *egl)
{
    egl->gl_exts = (const char *)egl->gl.GetString(GL_EXTENSIONS);
    if (!egl->gl_exts)
        egl_die("no GLES extensions");
}

static inline void
egl_init(struct egl *egl, const struct egl_init_params *params)
{
    memset(egl, 0, sizeof(*egl));

    if (params)
        egl->params = *params;

    egl_init_library(egl);
    egl_check(egl, "init library");

    egl_init_display(egl);
    egl_check(egl, "init display");

    egl_init_image_allocator(egl);
    egl_check(egl, "init image allocator");

    egl_init_config_and_surface(egl);
    egl_check(egl, "init config and surface");

    egl_init_context(egl);
    egl_check(egl, "init context");

    egl_init_drm_formats(egl);
    egl_check(egl, "init formats");

    egl_init_gl(egl);
    egl_check(egl, "init gl");
}

static inline void
egl_cleanup(struct egl *egl)
{
    egl_check(egl, "cleanup");

    if (egl->drm_format_count) {
        for (int i = 0; i < egl->drm_format_count; i++)
            free(egl->drm_formats[i]);
        free(egl->drm_formats);
    }

    egl->MakeCurrent(egl->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    egl->DestroyContext(egl->dpy, egl->ctx);
    if (egl->surf != EGL_NO_SURFACE)
        egl->DestroySurface(egl->dpy, egl->surf);

    egl_cleanup_image_allocator(egl);

    egl->Terminate(egl->dpy);
    egl->ReleaseThread();

    dlclose(egl->handle);
}

static inline void
egl_dump_drm_formats(struct egl *egl)
{
    for (int i = 0; i < egl->drm_format_count; i++) {
        const struct egl_drm_format *fmt = egl->drm_formats[i];

        egl_log("format %d: %c%c%c%c (0x%08x)", i, (fmt->drm_format >> 0) & 0xff,
                (fmt->drm_format >> 8) & 0xff, (fmt->drm_format >> 16) & 0xff,
                (fmt->drm_format >> 24) & 0xff, fmt->drm_format);
        for (int j = 0; j < fmt->drm_modifier_count; j++) {
            egl_log("  modifier 0x%016" PRIx64 " external only %d", fmt->drm_modifiers[j],
                    fmt->external_only[j]);
        }
    }
}

static inline void
egl_dump_image(struct egl *egl, int width, int height, const char *filename)
{
    const GLenum format = GL_RGBA;
    const GLenum type = GL_UNSIGNED_BYTE;
    const GLsizei size = width * height * 4;

    char *data = malloc(size);
    if (!data)
        egl_die("failed to alloc readback buf");

    egl->gl.ReadnPixels(0, 0, width, height, format, type, size, data);
    egl_check(egl, "dump");

    u_write_ppm(filename, data, width, height);

    free(data);
}

static inline void
egl_teximage_2d_from_ppm(struct egl *egl, GLenum target, const void *ppm_data, size_t ppm_size)
{
    struct egl_gl *gl = &egl->gl;

    int width;
    int height;
    ppm_data = u_parse_ppm(ppm_data, ppm_size, &width, &height);

    void *texels = malloc(width * height * 4);
    if (!texels)
        egl_die("failed to alloc texels");

    const uint8_t *rgb = ppm_data;
    uint8_t *rgba = texels;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            rgba[0] = rgb[0];
            rgba[1] = rgb[1];
            rgba[2] = rgb[2];
            rgba[3] = 0xff;
            rgba += 4;
            rgb += 3;
        }
    }

    gl->TexImage2D(target, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);

    free(texels);
}

static inline struct egl_framebuffer *
egl_create_framebuffer(struct egl *egl, int width, int height)
{
    struct egl_gl *gl = &egl->gl;

    struct egl_framebuffer *fb = calloc(1, sizeof(*fb));
    if (!fb)
        egl_die("failed to alloc fb");

    const GLenum target = GL_FRAMEBUFFER;
    const GLenum textarget = GL_TEXTURE_2D;
    const GLenum att = GL_COLOR_ATTACHMENT0;

    gl->GenTextures(1, &fb->tex);
    gl->BindTexture(textarget, fb->tex);
    gl->TexStorage2D(textarget, 1, GL_RGBA8, width, height);
    gl->BindTexture(textarget, 0);

    gl->GenFramebuffers(1, &fb->fbo);
    gl->BindFramebuffer(target, fb->fbo);
    gl->FramebufferTexture(target, att, fb->tex, 0);

    if (gl->CheckFramebufferStatus(target) != GL_FRAMEBUFFER_COMPLETE)
        egl_die("incomplete fbo");

    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);

    return fb;
}

static inline void
egl_destroy_framebuffer(struct egl *egl, struct egl_framebuffer *fb)
{
    struct egl_gl *gl = &egl->gl;

    gl->DeleteTextures(1, &fb->tex);
    gl->DeleteFramebuffers(1, &fb->fbo);

    free(fb);
}

static inline GLuint
egl_compile_shader(struct egl *egl, GLenum type, const char *glsl)
{
    struct egl_gl *gl = &egl->gl;

    GLuint sh = gl->CreateShader(type);
    gl->ShaderSource(sh, 1, &glsl, NULL);
    gl->CompileShader(sh);

    GLint val;
    gl->GetShaderiv(sh, GL_COMPILE_STATUS, &val);
    if (val != GL_TRUE) {
        char info_log[1024];
        gl->GetShaderInfoLog(sh, sizeof(info_log), NULL, info_log);
        egl_die("failed to compile shader: %s", info_log);
    }

    return sh;
}

static inline GLuint
egl_link_program(struct egl *egl, const GLuint *shaders, int count)
{
    struct egl_gl *gl = &egl->gl;

    GLuint prog = gl->CreateProgram();
    for (int i = 0; i < count; i++)
        gl->AttachShader(prog, shaders[i]);
    gl->LinkProgram(prog);

    GLint val;
    gl->GetProgramiv(prog, GL_LINK_STATUS, &val);
    if (val != GL_TRUE) {
        char info_log[1024];
        gl->GetProgramInfoLog(prog, sizeof(info_log), NULL, info_log);
        egl_die("failed to link program: %s", info_log);
    }

    return prog;
}

static inline struct egl_program *
egl_create_program(struct egl *egl, const char *vs_glsl, const char *fs_glsl)
{
    struct egl_program *prog = calloc(1, sizeof(*prog));
    if (!prog)
        egl_die("failed to alloc prog");

    prog->vs = egl_compile_shader(egl, GL_VERTEX_SHADER, vs_glsl);
    prog->fs = egl_compile_shader(egl, GL_FRAGMENT_SHADER, fs_glsl);

    const GLuint shaders[] = { prog->vs, prog->fs };
    prog->prog = egl_link_program(egl, shaders, ARRAY_SIZE(shaders));

    return prog;
}

static inline void
egl_destroy_program(struct egl *egl, struct egl_program *prog)
{
    struct egl_gl *gl = &egl->gl;

    gl->DeleteProgram(prog->prog);
    gl->DeleteShader(prog->vs);
    gl->DeleteShader(prog->fs);

    free(prog);
}

static inline void
egl_import_image(struct egl *egl, struct egl_image *img)
{
    const struct egl_image_info *info = &img->storage->info;

    EGLAttrib attrs[64];
    uint32_t attr_count = 0;

    attrs[attr_count++] = EGL_IMAGE_PRESERVED;
    attrs[attr_count++] = EGL_TRUE;

    switch (info->target) {
    case EGL_NATIVE_BUFFER_ANDROID:
        if (!egl->ANDROID_image_native_buffer)
            egl_die("no native buffer import support");
        break;
    case EGL_LINUX_DMA_BUF_EXT:
        if (!egl->EXT_image_dma_buf_import || !egl->EXT_image_dma_buf_import_modifiers)
            egl_die("no dma-buf import support");

        attrs[attr_count++] = EGL_WIDTH;
        attrs[attr_count++] = info->width;
        attrs[attr_count++] = EGL_HEIGHT;
        attrs[attr_count++] = info->height;
        attrs[attr_count++] = EGL_LINUX_DRM_FOURCC_EXT;
        attrs[attr_count++] = info->drm_format;

        if (info->mem_plane_count > 4)
            egl_die("unexpected plane count");
        for (int i = 0; i < info->mem_plane_count; i++) {
            if (i < 3) {
                attrs[attr_count++] = EGL_DMA_BUF_PLANE0_FD_EXT + 3 * i;
                attrs[attr_count++] = info->dma_buf_fd;
                attrs[attr_count++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT + 3 * i;
                attrs[attr_count++] = info->offsets[i];
                attrs[attr_count++] = EGL_DMA_BUF_PLANE0_PITCH_EXT + 3 * i;
                attrs[attr_count++] = info->pitches[i];
            } else {
                attrs[attr_count++] = EGL_DMA_BUF_PLANE3_FD_EXT;
                attrs[attr_count++] = info->dma_buf_fd;
                attrs[attr_count++] = EGL_DMA_BUF_PLANE3_OFFSET_EXT;
                attrs[attr_count++] = info->offsets[i];
                attrs[attr_count++] = EGL_DMA_BUF_PLANE3_PITCH_EXT;
                attrs[attr_count++] = info->pitches[i];
            }
            attrs[attr_count++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT + 2 * i;
            attrs[attr_count++] = (EGLAttrib)info->drm_modifier;
            attrs[attr_count++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT + 2 * i;
            attrs[attr_count++] = (EGLAttrib)(info->drm_modifier >> 32);
        }
        break;
    default:
        egl_die("bad image target");
        break;
    }

    attrs[attr_count++] = EGL_NONE;
    assert(attr_count <= ARRAY_SIZE(attrs));

    img->img = egl->CreateImage(egl->dpy, info->ctx, info->target, info->buf, attrs);
    if (img->img == EGL_NO_IMAGE)
        egl_die("failed to create img");
}

static inline struct egl_image *
egl_create_image(struct egl *egl, const struct egl_image_storage_info *info)
{
    struct egl_image_storage *storage = egl_alloc_image_storage(egl, info);

    struct egl_image *img = calloc(1, sizeof(*img));
    if (!img)
        egl_die("failed to alloc img");

    img->storage = storage;
    egl_import_image(egl, img);

    return img;
}

static inline struct egl_image *
egl_create_image_from_ppm(struct egl *egl, const void *ppm_data, size_t ppm_size, bool planar)
{
    int width;
    int height;
    ppm_data = u_parse_ppm(ppm_data, ppm_size, &width, &height);

    if (planar && egl->gbm && !egl->is_minigbm)
        egl_die("only minigbm supports planar formats");

    const struct egl_image_storage_info storage_info = {
        .width = width,
        .height = height,
        .drm_format = planar ? DRM_FORMAT_NV12 : DRM_FORMAT_ABGR8888,
        .mapping = true,
        .rendering = false,
        .sampling = true,

        /* When mapping, gbm or gralloc is supposed to give us a linear view
         * even when the image is tiled.  mesa gbm does not support planar
         * formats.  minigbm has quirks:
         *
         *  - its i915 backend can pick modifiers with compressions and refuse
         *    to map them
         *  - its amdgpu backend uses DRI unless we force linear
         *  - its msm backend does not give a linear view
         */
        .force_linear = egl->is_minigbm,
    };
    struct egl_image *img = egl_create_image(egl, &storage_info);
    struct egl_image_storage *storage = img->storage;

    egl_map_image_storage(egl, storage);

    struct u_format_conversion conv = {
        .width = width,
        .height = height,

        .src_format = DRM_FORMAT_BGR888,
        .src_plane_count = 1,
        .src_plane_ptrs = { ppm_data, },
        .src_plane_strides = { width * 3, },

        .dst_format = planar ? DRM_FORMAT_NV12 : DRM_FORMAT_ABGR8888,
        .dst_plane_count = planar ? 2 : 1,
    };
    for (int i = 0; i < conv.dst_plane_count; i++) {
        conv.dst_plane_ptrs[i] = storage->planes[i];
        conv.dst_plane_strides[i] = storage->strides[i];
    }

    u_convert_format(&conv);

    egl_unmap_image_storage(egl, storage);

    return img;
}

static inline void
egl_destroy_image(struct egl *egl, struct egl_image *img)
{
    egl->DestroyImage(egl->dpy, img->img);
    egl_free_image_storage(egl, img->storage);
    free(img);
}

#endif /* EGLUTIL_H */
