/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef UTIL_H
#define UTIL_H

#include "drm/drm_fourcc.h"

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define PRINTFLIKE(f, a) __attribute__((format(printf, f, a)))
#define NORETURN __attribute__((noreturn))

#define _STRINGIFY(v) #v
#define STRINGIFY(v) _STRINGIFY(v)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define ALIGN(v, a) (((v) + (a) - 1) & ~((a) - 1))
#define DIV_ROUND_UP(v, d) (((v) + (d) - 1) / (d))

static inline bool
u_isatty(void)
{
    return isatty(STDOUT_FILENO);
}

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
u_map_file(const char *filename, size_t *out_size)
{
    const int fd = open(filename, O_RDONLY);
    if (fd < 0)
        u_die("util", "failed to open %s", filename);

    const off_t size = lseek(fd, 0, SEEK_END);
    if (size < 0)
        u_die("util", "failed to seek file");

    const void *ptr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED)
        u_die("util", "failed to map file");

    close(fd);

    *out_size = size;
    return ptr;
}

static inline void
u_unmap_file(const void *ptr, size_t size)
{
    munmap((void *)ptr, size);
}

static inline const void *
u_parse_ppm(const void *ppm_data, size_t ppm_size, uint32_t *width, uint32_t *height)
{
    int w;
    int h;
    if (sscanf((const char *)ppm_data, "P6 %d %d 255\n", &w, &h) != 2)
        u_die("util", "invalid ppm header");

    const size_t img_size = w * h * 3;
    if (img_size >= ppm_size)
        u_die("util", "bad ppm dimension %dx%d", w, h);

    const size_t hdr_size = ppm_size - img_size;
    if (!isspace(((const char *)ppm_data)[hdr_size - 1]))
        u_die("util", "no space at the end of ppm header");

    *width = w;
    *height = h;
    return (const char *)ppm_data + hdr_size;
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
            const void *pixel = (const char *)data + ((width * y) + x) * 4;
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

struct u_format_conversion {
    uint32_t width;
    uint32_t height;

    uint32_t src_format;
    uint32_t src_plane_count;
    const void *src_plane_ptrs[3];
    uint32_t src_plane_strides[3];

    uint32_t dst_format;
    uint32_t dst_plane_count;
    void *dst_plane_ptrs[3];
    uint32_t dst_plane_strides[3];
};

static inline void
u_convert_format(const struct u_format_conversion *conv)
{
    if (conv->src_format != DRM_FORMAT_BGR888)
        u_die("util", "unsupported src format");
    if (conv->src_plane_count != 1)
        u_die("util", "bad src plane count");

    switch (conv->dst_format) {
    case DRM_FORMAT_ABGR8888:
        if (conv->dst_plane_count != 1)
            u_die("util", "bad dst plane count");

        for (uint32_t y = 0; y < conv->height; y++) {
            const uint8_t *src =
                (const uint8_t *)conv->src_plane_ptrs[0] + conv->src_plane_strides[0] * y;
            uint8_t *dst = (uint8_t *)conv->dst_plane_ptrs[0] + conv->dst_plane_strides[0] * y;
            for (uint32_t x = 0; x < conv->width; x++) {
                memcpy(dst, src, 3);
                dst[3] = 0xff;

                src += 3;
                dst += 4;
            }
        }
        break;
    case DRM_FORMAT_NV12:
        if (conv->dst_plane_count != 2)
            u_die("util", "bad dst plane count");

        /* be careful about 4:2:0 subsampling */
        for (uint32_t y = 0; y < conv->height; y++) {
            const uint8_t *src =
                (const uint8_t *)conv->src_plane_ptrs[0] + conv->src_plane_strides[0] * y;
            uint8_t *dst_y = (uint8_t *)conv->dst_plane_ptrs[0] + conv->dst_plane_strides[0] * y;
            uint8_t *dst_uv =
                (y & 1) ? NULL
                        : (uint8_t *)conv->dst_plane_ptrs[1] + conv->dst_plane_strides[1] * y / 2;

            for (uint32_t x = 0; x < conv->width; x++) {
                uint8_t yuv[3];
                u_rgb_to_yuv(src, yuv);
                src += 3;

                dst_y[0] = yuv[0];
                dst_y += 1;

                if (dst_uv && !(x & 1)) {
                    dst_uv[0] = yuv[1];
                    dst_uv[1] = yuv[2];
                    dst_uv += 2;
                }
            }
        }
        break;
    default:
        u_die("util", "unsupported dst format");
        break;
    }
}

static inline uint32_t
u_drm_format_to_plane_count(uint32_t drm_format)
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

static inline uint32_t
u_drm_format_to_plane_format(uint32_t drm_format, uint32_t plane)
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

static inline uint32_t
u_drm_format_to_cpp(uint32_t drm_format)
{
    switch (drm_format) {
    case DRM_FORMAT_ABGR16161616F:
        return 8;
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XRGB8888:
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
    default:
        /* cpp makes no sense to planar formats */
        if (u_drm_format_to_plane_count(drm_format) > 1)
            return 0;
        u_die("util", "unsupported drm format 0x%x", drm_format);
        break;
    }
}

#endif /* UTIL_H */
