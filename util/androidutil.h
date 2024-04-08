/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef ANDROIDUTIL_H
#define ANDROIDUTIL_H

#include "util.h"

#include <android/hardware_buffer.h>

struct android_init_params {
    int unused;
};

struct android {
    struct android_init_params params;
};

struct android_ahb {
    AHardwareBuffer *ahb;
    AHardwareBuffer_Desc desc;
};

static inline void PRINTFLIKE(1, 2) android_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_logv("ANDROID", format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(1, 2) NORETURN android_die(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_diev("ANDROID", format, ap);
    va_end(ap);
}

static inline void
android_init(struct android *android, const struct android_init_params *params)
{
    memset(android, 0, sizeof(*android));
    if (params)
        android->params = *params;
}

static inline void
android_cleanup(struct android *android)
{
}

static const struct {
    enum AHardwareBuffer_Format ahb_format;
    uint32_t drm_format;
} android_ahb_format_table[] = {
    {
        AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
        DRM_FORMAT_ABGR8888,
    },
    {
        AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM,
        DRM_FORMAT_XBGR8888,
    },
    {
        AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM,
        DRM_FORMAT_BGR888,
    },
    {
        AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM,
        DRM_FORMAT_RGB565,
    },
    {
        AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT,
        DRM_FORMAT_ABGR16161616F,
    },
    {
        AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM,
        DRM_FORMAT_ABGR2101010,
    },
    {
        AHARDWAREBUFFER_FORMAT_BLOB,
        0,
    },
    {
        AHARDWAREBUFFER_FORMAT_D16_UNORM,
        0,
    },
    {
        AHARDWAREBUFFER_FORMAT_D24_UNORM,
        0,
    },
    {
        AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT,
        0,
    },
    {
        AHARDWAREBUFFER_FORMAT_D32_FLOAT,
        0,
    },
    {
        AHARDWAREBUFFER_FORMAT_D32_FLOAT_S8_UINT,
        0,
    },
    {
        AHARDWAREBUFFER_FORMAT_S8_UINT,
        0,
    },
#if __ANDROID_API__ >= 29
    /* AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420 is flexible and is not 1:1 */
    {
        AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420,
        DRM_FORMAT_NV12,
    },
    {
        AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420,
        DRM_FORMAT_YVU420,
    },
    {
        AHARDWAREBUFFER_FORMAT_YCbCr_P010,
        DRM_FORMAT_P010,
    },
#endif
    {
        AHARDWAREBUFFER_FORMAT_R8_UNORM,
        DRM_FORMAT_R8,
    },
    {
        AHARDWAREBUFFER_FORMAT_R16_UINT,
        DRM_FORMAT_R16,
    },
    {
        AHARDWAREBUFFER_FORMAT_R16G16_UINT,
        DRM_FORMAT_GR1616,
    },
    {
        AHARDWAREBUFFER_FORMAT_R10G10B10A10_UNORM,
        DRM_FORMAT_AXBXGXRX106106106106,
    },
};

static inline enum AHardwareBuffer_Format
android_ahb_format_from_drm_format(uint32_t drm_format)
{
    for (uint32_t i = 0; i < ARRAY_SIZE(android_ahb_format_table); i++) {
        if (android_ahb_format_table[i].drm_format == drm_format)
            return android_ahb_format_table[i].ahb_format;
    }

    android_die("unknown drm format '%.*s'", 4, (const char *)&drm_format);
}

static inline uint32_t
android_ahb_format_to_drm_format(enum AHardwareBuffer_Format ahb_format)
{
    for (uint32_t i = 0; i < ARRAY_SIZE(android_ahb_format_table); i++) {
        if (android_ahb_format_table[i].ahb_format == ahb_format)
            return android_ahb_format_table[i].drm_format;
    }

    android_die("unknown ahb format 0x%x", ahb_format);
}

static inline struct android_ahb *
android_create_ahb(struct android *android,
                   uint32_t width,
                   uint32_t height,
                   enum AHardwareBuffer_Format format,
                   uint64_t usage)
{
    struct android_ahb *ahb = calloc(1, sizeof(*ahb));
    if (!ahb)
        android_die("failed to alloc ahb");

    const AHardwareBuffer_Desc desc = {
        .width = width,
        .height = height,
        .layers = 1,
        .format = format,
        .usage = usage,
    };
    if (AHardwareBuffer_allocate(&desc, &ahb->ahb))
        android_die("failed to allocate ahb");

    AHardwareBuffer_describe(ahb->ahb, &ahb->desc);

    if (ahb->desc.width != desc.width || ahb->desc.height != desc.height ||
        ahb->desc.layers != desc.layers || ahb->desc.format != desc.format ||
        ahb->desc.usage != desc.usage)
        android_die("unexpected ahb desc change");

    return ahb;
}

static inline void
android_destroy_ahb(struct android *android, struct android_ahb *ahb)
{
    AHardwareBuffer_release(ahb->ahb);
    free(ahb);
}

static inline void
android_map_ahb(struct android *android, struct android_ahb *ahb, AHardwareBuffer_Planes *planes)
{
    const uint64_t usage =
        AHARDWAREBUFFER_USAGE_CPU_READ_RARELY | AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY;
    const ARect rect = { .right = ahb->desc.width, .bottom = ahb->desc.height };

#if __ANDROID_API__ >= 29
    if (AHardwareBuffer_lockPlanes(ahb->ahb, usage, -1, &rect, planes))
        android_die("failed to lock ahb");
#else
    void *ptr;
    if (AHardwareBuffer_lock(ahb->ahb, usage, -1, &rect, &ptr))
        android_die("failed to lock ahb");

    const uint32_t drm_format = android_ahb_format_to_drm_format(ahb->desc.format);
    const uint32_t plane_count = u_drm_format_to_plane_count(drm_format);
    if (plane_count != 1)
        android_die("failed to lock planar ahb");
    const uint32_t cpp = u_drm_format_to_cpp(drm_format);

    planes->planeCount = 1;
    planes->planes[0].data = ptr;
    planes->planes[0].pixelStride = cpp;
    planes->planes[0].rowStride = ahb->desc.stride * cpp;
#endif
}

static inline void
android_unmap_ahb(struct android *android, struct android_ahb *ahb)
{
    AHardwareBuffer_unlock(ahb->ahb, NULL);
}

static inline void
android_convert_ahb_planes(struct android *android,
                           uint32_t drm_format,
                           AHardwareBuffer_Planes *planes)
{
    if (drm_format != DRM_FORMAT_NV12)
        android_die("bad drm format");

    if (planes->planeCount != 3 || planes->planes[1].rowStride != planes->planes[2].rowStride ||
        planes->planes[1].pixelStride != 2 || planes->planes[2].pixelStride != 2 ||
        planes->planes[1].data + 1 != planes->planes[2].data)
        android_die("ahb is not in NV12");

    planes->planeCount = 2;
}

static inline struct android_ahb *
android_create_ahb_from_ppm(struct android *android,
                            const void *ppm_data,
                            size_t ppm_size,
                            enum AHardwareBuffer_Format format,
                            uint64_t usage)
{
    int width;
    int height;
    ppm_data = u_parse_ppm(ppm_data, ppm_size, &width, &height);

    const uint32_t drm_format = android_ahb_format_to_drm_format(format);
    if (drm_format != DRM_FORMAT_NV12 && drm_format != DRM_FORMAT_ABGR8888)
        android_die("unsupported target format");

    struct android_ahb *ahb = android_create_ahb(android, width, height, format, usage);

    AHardwareBuffer_Planes planes;
    android_map_ahb(android, ahb, &planes);
    if (drm_format == DRM_FORMAT_NV12) {
        android_convert_ahb_planes(android, drm_format, &planes);
    }

    struct u_format_conversion conv = {
        .width = ahb->desc.width,
        .height = ahb->desc.height,

        .src_format = DRM_FORMAT_BGR888,
        .src_plane_count = 1,
        .src_plane_ptrs = { ppm_data, },
        .src_plane_strides = { width * 3, },

        .dst_format = drm_format,
        .dst_plane_count = u_drm_format_to_plane_count(drm_format),
    };
    if (conv.dst_plane_count != (int)planes.planeCount)
        android_die("unexpected plane count");
    for (int i = 0; i < conv.dst_plane_count; i++) {
        conv.dst_plane_ptrs[i] = planes.planes[i].data;
        conv.dst_plane_strides[i] = planes.planes[i].rowStride;
    }

    u_convert_format(&conv);

    android_unmap_ahb(android, ahb);

    return ahb;
}

#endif /* ANDROIDUTIL_H */
