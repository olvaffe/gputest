/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef UTIL_H
#define UTIL_H

#include "drm/drm_fourcc.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PRINTFLIKE(f, a) __attribute__((format(printf, f, a)))
#define NORETURN __attribute__((noreturn))

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define ALIGN(v, a) (((v) + (a) - 1) & ~((a) - 1))
#define DIV_ROUND_UP(v, d) (((v) + (d) - 1) / (d))

static inline void
u_logv(const char *tag, const char *format, va_list ap)
{
    printf("%s: ", tag);
    vprintf(format, ap);
    printf("\n");
}

static inline void NORETURN
u_diev(const char *tag, const char *format, va_list ap)
{
    u_logv(tag, format, ap);
    abort();
}

static inline void PRINTFLIKE(2, 3) u_log(const char *tag, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_logv(tag, format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(2, 3) NORETURN u_die(const char *tag, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_diev(tag, format, ap);
    va_end(ap);
}

struct u_bitmask_desc {
    uint64_t bitmask;
    const char *str;
};

static inline const char *
u_bitmask_to_str(
    uint64_t bitmask, const struct u_bitmask_desc *descs, uint32_t count, char *str, size_t size)
{
    int len = 0;
    for (uint32_t i = 0; i < count; i++) {
        const struct u_bitmask_desc *desc = &descs[i];
        if (bitmask & desc->bitmask) {
            len += snprintf(str + len, size - len, "%s|", desc->str);
            bitmask &= ~desc->bitmask;
        }
    }

    if (bitmask)
        snprintf(str + len, size - len, "0x%" PRIx64, bitmask);
    else if (len)
        str[len - 1] = '\0';
    else
        snprintf(str + len, size - len, "none");

    return str;
}

static inline uint32_t
u_minify(uint32_t base, uint32_t level)
{
    const uint32_t val = base >> level;
    return val ? val : 1;
}

static inline uint64_t
u_now(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts))
        return 0;

    const uint64_t ns = 1000000000ull;
    return ns * ts.tv_sec + ts.tv_nsec;
}

static inline void
u_sleep(uint32_t ms)
{
    const struct timespec ts = {
        .tv_sec = ms / 1000,
        .tv_nsec = (ms % 1000) * 1000000,
    };

    const int ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
    if (ret)
        u_die("util", "failed to sleep");
}

static inline const void *
u_parse_ppm(const void *ppm_data, size_t ppm_size, int *width, int *height)
{
    if (sscanf(ppm_data, "P6 %d %d 255\n", width, height) != 2)
        u_die("util", "invalid ppm header");

    const size_t img_size = *width * *height * 3;
    if (img_size >= ppm_size)
        u_die("util", "bad ppm dimension %dx%d", *width, *height);

    const size_t hdr_size = ppm_size - img_size;
    if (!isspace(((const char *)ppm_data)[hdr_size - 1]))
        u_die("util", "no space at the end of ppm header");

    return ppm_data + hdr_size;
}

static inline void
u_write_ppm(const char *filename, const void *data, int width, int height)
{
    FILE *fp = fopen(filename, "w");
    if (!fp)
        u_die("util", "failed to open %s", filename);

    fprintf(fp, "P6 %d %d 255\n", width, height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const void *pixel = data + ((width * y) + x) * 4;
            if (fwrite(pixel, 3, 1, fp) != 1)
                u_die("util", "failed to write pixel (%d, %x)", x, y);
        }
    }

    fclose(fp);
}

static inline void
u_rgb_to_yuv(const uint8_t *rgb, uint8_t *yuv)
{
    const int tmp[3] = {
        ((66 * (rgb)[0] + 129 * (rgb)[1] + 25 * (rgb)[2] + 128) >> 8) + 16,
        ((-38 * (rgb)[0] - 74 * (rgb)[1] + 112 * (rgb)[2] + 128) >> 8) + 128,
        ((112 * (rgb)[0] - 94 * (rgb)[1] - 18 * (rgb)[2] + 128) >> 8) + 128,
    };

    for (int i = 0; i < 3; i++) {
        if (tmp[i] > 255)
            yuv[i] = 255;
        else if (tmp[i] < 0)
            yuv[i] = 0;
        else
            yuv[i] = tmp[i];
    }
}

static inline int
u_drm_format_to_cpp(int drm_format)
{
    switch (drm_format) {
    case DRM_FORMAT_ABGR16161616F:
        return 8;
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_ABGR2101010:
    case DRM_FORMAT_GR1616:
        return 4;
    case DRM_FORMAT_BGR888:
        return 3;
    case DRM_FORMAT_RGB565:
    case DRM_FORMAT_GR88:
    case DRM_FORMAT_R16:
        return 2;
    case DRM_FORMAT_R8:
        return 1;
    case DRM_FORMAT_P010:
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_YVU420:
        /* cpp makes no sense to planar formats */
        return 0;
    default:
        u_die("util", "unsupported drm format 0x%x", drm_format);
    }
}

static inline int
u_drm_format_to_plane_count(int drm_format)
{
    switch (drm_format) {
    case DRM_FORMAT_YVU420:
        return 3;
    case DRM_FORMAT_P010:
    case DRM_FORMAT_NV12:
        return 2;
    default:
        return 1;
    }
}

static inline int
u_drm_format_to_plane_format(int drm_format, int plane)
{
    if (plane >= u_drm_format_to_plane_count(drm_format))
        u_die("util", "bad plane");

    switch (drm_format) {
    case DRM_FORMAT_YVU420:
        return DRM_FORMAT_R8;
    case DRM_FORMAT_P010:
        return plane ? DRM_FORMAT_GR1616 : DRM_FORMAT_R16;
    case DRM_FORMAT_NV12:
        return plane ? DRM_FORMAT_GR88 : DRM_FORMAT_R8;
    default:
        return drm_format;
    }
}

#endif /* UTIL_H */
