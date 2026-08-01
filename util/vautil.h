/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef VAUTIL_H
#define VAUTIL_H

#include "util.h"

#include <va/va.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>
#include <va/va_str.h>
#include <xf86drm.h>

#define va_die(format, ...) u_die("VA", format __VA_OPT__(, ) __VA_ARGS__)
#define va_log(format, ...) u_log("VA", format __VA_OPT__(, ) __VA_ARGS__)
#define va_check(va, format, ...)                                                                \
    u_check("VA", (va)->status == VA_STATUS_SUCCESS, format __VA_OPT__(, ) __VA_ARGS__)

struct va_init_params {
    int drm_fd;
};

struct va_pair {
    VAProfile profile;
    VAEntrypoint entrypoint;

    VAConfigAttrib attrs[VAConfigAttribTypeMax];
};

struct va {
    struct va_init_params params;

    VAStatus status;

    int native_display;
    VADisplay display;
    int major;
    int minor;
    const char *vendor;
    VADisplayAttribute *attrs;
    int attr_count;

    struct va_pair *pairs;
    int pair_count;

    VAImageFormat *img_formats;
    unsigned int img_count;

    VAImageFormat *subpic_formats;
    unsigned int *subpic_flags;
    unsigned int subpic_count;
};

static inline void
va_init_display(struct va *va)
{
    va->native_display = va->params.drm_fd;

    va->display = vaGetDisplayDRM(va->native_display);
    if (!va->display)
        va_die("failed to get display");

    va->status = vaInitialize(va->display, &va->major, &va->minor);
    va_check(va, "failed to initialize display: %d (no driver?)", va->status);

    va->vendor = vaQueryVendorString(va->display);

    const int attr_max = vaMaxNumDisplayAttributes(va->display);
    va->attrs = malloc(sizeof(*va->attrs) * attr_max);
    if (!va->attrs)
        va_die("failed to alloc display attrs");
    va->status = vaQueryDisplayAttributes(va->display, va->attrs, &va->attr_count);
    va_check(va, "failed to query display attrs");
}

static inline void
va_init_pairs(struct va *va)
{
    const int profile_max = vaMaxNumProfiles(va->display);
    VAProfile *profiles = malloc(sizeof(*profiles) * profile_max);
    if (!profiles)
        va_die("failed to alloc profiles");

    const int entrypoint_max = vaMaxNumEntrypoints(va->display);
    VAEntrypoint *entrypoints = malloc(sizeof(*entrypoints) * entrypoint_max);
    if (!entrypoints)
        va_die("failed to alloc entrypoints");

    int profile_count;
    va->status = vaQueryConfigProfiles(va->display, profiles, &profile_count);
    va_check(va, "failed to query profiles");

    for (int i = 0; i < profile_count; i++) {
        int entrypoint_count;
        va->status =
            vaQueryConfigEntrypoints(va->display, profiles[i], entrypoints, &entrypoint_count);
        va_check(va, "failed to query entrypoints");
        va->pair_count += entrypoint_count;
    }

    va->pairs = malloc(sizeof(*va->pairs) * va->pair_count);
    if (!va->pairs)
        va_die("failed to alloc pairs");

    struct va_pair *pair = va->pairs;
    for (int i = 0; i < profile_count; i++) {
        int entrypoint_count;
        va->status =
            vaQueryConfigEntrypoints(va->display, profiles[i], entrypoints, &entrypoint_count);
        va_check(va, "failed to query entrypoints");

        for (int j = 0; j < entrypoint_count; j++) {
            assert(pair < va->pairs + va->pair_count);

            pair->profile = profiles[i];
            pair->entrypoint = entrypoints[j];
            for (int k = 0; k < VAConfigAttribTypeMax; k++)
                pair->attrs[k].type = k;

            va->status = vaGetConfigAttributes(va->display, pair->profile, pair->entrypoint,
                                               pair->attrs, VAConfigAttribTypeMax);
            va_check(va, "failed to get config attrs");

            pair++;
        }
    }

    free(profiles);
    free(entrypoints);
}

static inline void
va_init_images(struct va *va)
{
    const int format_max = vaMaxNumImageFormats(va->display);

    va->img_formats = malloc(sizeof(*va->img_formats) * format_max);
    if (!va->img_formats)
        va_die("failed to alloc img formats");

    va->status = vaQueryImageFormats(va->display, va->img_formats, (int *)&va->img_count);
    va_check(va, "failed to query img formats");
}

static inline void
va_init_subpics(struct va *va)
{
    const int format_max = vaMaxNumSubpictureFormats(va->display);

    va->subpic_formats =
        malloc((sizeof(*va->subpic_formats) + sizeof(*va->subpic_flags)) * format_max);
    if (!va->subpic_formats)
        va_die("failed to alloc subpic formats");
    va->subpic_flags = (void *)(va->subpic_formats + format_max);

    va->status = vaQuerySubpictureFormats(va->display, va->subpic_formats, va->subpic_flags,
                                          &va->subpic_count);
    va_check(va, "failed to query subpic formats");
}

static inline void
va_init(struct va *va, const struct va_init_params *params)
{
    memset(va, 0, sizeof(*va));
    va->params = *params;

    va_init_display(va);
    va_init_pairs(va);
    va_init_images(va);
    va_init_subpics(va);
}

static inline void
va_cleanup(struct va *va)
{
    free(va->subpic_formats);
    free(va->img_formats);
    free(va->pairs);
    free(va->attrs);

    vaTerminate(va->display);
    close(va->native_display);
}

static inline const struct va_pair *
va_find_pair(const struct va *va, VAProfile profile, VAEntrypoint entrypoint)
{
    for (int i = 0; i < va->pair_count; i++) {
        const struct va_pair *pair = &va->pairs[i];
        if (pair->profile == profile && pair->entrypoint == entrypoint)
            return pair;
    }
    return NULL;
}

static inline VAConfigID
va_create_config(struct va *va,
                 VAProfile profile,
                 VAEntrypoint entrypoint,
                 unsigned int rt_formats)
{
    VAConfigAttrib attrs[VAConfigAttribTypeMax];
    int attr_count = 0;

    attrs[attr_count].type = VAConfigAttribRTFormat;
    attrs[attr_count++].value = rt_formats;

    VAConfigID config;
    va->status = vaCreateConfig(va->display, profile, entrypoint, attrs, attr_count, &config);
    va_check(va, "failed to create config");

    return config;
}

static inline void
va_destroy_config(struct va *va, VAConfigID config)
{
    va->status = vaDestroyConfig(va->display, config);
    va_check(va, "failed to destroy config");
}

static inline VASurfaceID
va_create_surface(
    struct va *va, unsigned int rt_format, unsigned int width, unsigned int height, int fourcc)
{
    VASurfaceAttrib attrs[VASurfaceAttribCount];
    int attr_count = 0;

    attrs[attr_count].type = VASurfaceAttribPixelFormat;
    attrs[attr_count].value.type = VAGenericValueTypeInteger;
    attrs[attr_count++].value.value.i = fourcc;

    VASurfaceID surf;
    va->status =
        vaCreateSurfaces(va->display, rt_format, width, height, &surf, 1, attrs, attr_count);
    va_check(va, "failed to create surface");

    return surf;
}

static inline void
va_destroy_surface(struct va *va, VASurfaceID surf)
{
    va->status = vaDestroySurfaces(va->display, &surf, 1);
    va_check(va, "failed to destroy surface");
}

static inline void
va_sync_surface(struct va *va, VASurfaceID surf)
{
    va->status = vaSyncSurface(va->display, surf);
    va_check(va, "failed to sync surface");
}

static inline void
va_export_surface(struct va *va,
                  VASurfaceID surf,
                  uint32_t flags,
                  VADRMPRIMESurfaceDescriptor *desc)
{
    va->status = vaExportSurfaceHandle(va->display, surf, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                       flags, desc);
    va_check(va, "failed to export surface");
}

static inline VAContextID
va_create_context(
    struct va *va, VAConfigID config, int width, int height, int flag, VASurfaceID surf)
{
    VAContextID ctx;
    va->status = vaCreateContext(va->display, config, width, height, flag, &surf, 1, &ctx);
    va_check(va, "failed to create context");

    return ctx;
}

static inline void
va_destroy_context(struct va *va, VAContextID ctx)
{
    va->status = vaDestroyContext(va->display, ctx);
    va_check(va, "failed to destroy context");
}

static inline VABufferID
va_create_buffer(
    struct va *va, VAContextID ctx, VABufferType type, unsigned int size, const void *data)
{
    VABufferID buf;
    va->status = vaCreateBuffer(va->display, ctx, type, size, 1, (void *)data, &buf);
    va_check(va, "failed to create buffer");

    return buf;
}

static inline void
va_destroy_buffer(struct va *va, VABufferID buf)
{
    va->status = vaDestroyBuffer(va->display, buf);
    va_check(va, "failed to destroy buffer");
}

static inline void *
va_map_buffer(struct va *va, VABufferID buf)
{
    void *ptr;
    va->status = vaMapBuffer(va->display, buf, &ptr);
    va_check(va, "failed to map buffer");
    return ptr;
}

static inline void
va_unmap_buffer(struct va *va, VABufferID buf)
{
    va->status = vaUnmapBuffer(va->display, buf);
    va_check(va, "failed to unmap buffer");
}

static inline void
va_begin_picture(struct va *va, VAContextID ctx, VASurfaceID surf)
{
    va->status = vaBeginPicture(va->display, ctx, surf);
    va_check(va, "failed to begin picture");
}

static inline void
va_render_picture(struct va *va, VAContextID ctx, const VABufferID *bufs, int count)
{
    va->status = vaRenderPicture(va->display, ctx, (VABufferID *)bufs, count);
    va_check(va, "failed to render picture");
}

static inline void
va_end_picture(struct va *va, VAContextID ctx)
{
    va->status = vaEndPicture(va->display, ctx);
    va_check(va, "failed to end picture");
}

static inline void
va_create_image(struct va *va, int width, int height, int fourcc, VAImage *img)
{
    VAImageFormat format = {
        .fourcc = fourcc,
    };

    va->status = vaCreateImage(va->display, &format, width, height, img);
    va_check(va, "failed to create image");
}

static inline void
va_destroy_image(struct va *va, VAImageID img)
{
    va->status = vaDestroyImage(va->display, img);
    va_check(va, "failed to destroy image");
}

static inline void
va_get_image(
    struct va *va, VASurfaceID surf, unsigned int width, unsigned int height, VAImageID img)
{
    va->status = vaGetImage(va->display, surf, 0, 0, width, height, img);
    va_check(va, "failed to get image");
}

static inline void
va_save_image(struct va *va, const VAImage *img, const char *filename)
{
    uint32_t drm_format = 0;
    uint32_t plane_count = 1;

    switch (img->format.fourcc) {
    case VA_FOURCC_NV12:
        drm_format = DRM_FORMAT_NV12;
        plane_count = 2;
        break;
    case VA_FOURCC_444P:
        drm_format = DRM_FORMAT_YUV444;
        plane_count = 3;
        break;
    case VA_FOURCC_AYUV:
        drm_format = DRM_FORMAT_AYUV;
        plane_count = 1;
        break;
    default:
        va_die("unsupported fourcc 0x%08x", img->format.fourcc);
        break;
    }

    void *ptr = va_map_buffer(va, img->buf);

    const uint32_t rgb_stride = img->width * 3;
    void *rgb_buf = malloc((size_t)rgb_stride * img->height);
    if (!rgb_buf)
        va_die("failed to alloc rgb buffer");

    struct u_format_conversion conv = {
        .width = img->width,
        .height = img->height,
        .src_format = drm_format,
        .src_plane_count = plane_count,
        .dst_format = DRM_FORMAT_RGB888,
        .dst_plane_count = 1,
        .dst_plane_ptrs = { rgb_buf },
        .dst_plane_strides = { rgb_stride },
    };

    for (uint32_t i = 0; i < plane_count; i++) {
        conv.src_plane_ptrs[i] = (const uint8_t *)ptr + img->offsets[i];
        conv.src_plane_strides[i] = img->pitches[i];
    }

    u_convert_format(&conv);

    FILE *fp = fopen(filename, "wb");
    if (!fp)
        va_die("failed to open %s", filename);

    fprintf(fp, "P6 %u %u 255\n", img->width, img->height);
    if (fwrite(rgb_buf, (size_t)rgb_stride * img->height, 1, fp) != 1)
        va_die("failed to write ppm pixels");

    fclose(fp);
    free(rgb_buf);

    va_unmap_buffer(va, img->buf);
}

#endif /* VAUTIL_H */
