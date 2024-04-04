/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef GBMUTIL_H
#define GBMUTIL_H

#include "util.h"

#include <fcntl.h>
#include <gbm.h>
#include <sys/stat.h>
#include <unistd.h>

struct gbm_format_info {
    uint32_t format;
    uint32_t flags;
    uint64_t *modifiers;
    uint32_t modifier_count;
};

struct gbm_init_params {
    const char *path;
};

struct gbm {
    struct gbm_init_params params;

    int fd;
    struct gbm_device *dev;
    const char *backend_name;

    struct gbm_format_info *formats;
    uint32_t format_count;
};

struct gbm_bo_info {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint64_t modifier;
    uint32_t bpp;

    uint32_t offsets[GBM_MAX_PLANES];
    uint32_t strides[GBM_MAX_PLANES];
    uint32_t plane_count;

    void *map_data;
};

static inline void PRINTFLIKE(1, 2) gbm_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_logv("GBM", format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(1, 2) NORETURN gbm_die(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_diev("GBM", format, ap);
    va_end(ap);
}

static inline const char *
gbm_flags_to_str(uint32_t val, char *str, size_t size)
{
    /* clang-format off */
    static const struct u_bitmask_desc descs[] = {
#define DESC(v) { .bitmask = GBM_BO_USE_ ##v, .str = #v }
        DESC(SCANOUT),
        DESC(CURSOR),
        DESC(RENDERING),
        DESC(WRITE),
        DESC(LINEAR),
        DESC(PROTECTED),
        DESC(FRONT_RENDERING),
#ifdef MINIGBM
        DESC(TEXTURING),
        DESC(CAMERA_WRITE),
        DESC(CAMERA_READ),
        DESC(SW_READ_OFTEN),
        DESC(SW_READ_RARELY),
        DESC(SW_WRITE_OFTEN),
        DESC(SW_WRITE_RARELY),
        DESC(HW_VIDEO_DECODER),
        DESC(HW_VIDEO_ENCODER),
        DESC(GPU_DATA_BUFFER),
        DESC(SENSOR_DIRECT_DATA),
#endif
#undef DESC
    };
    /* clang-format on */

    return u_bitmask_to_str(val, descs, ARRAY_SIZE(descs), str, size);
}

static inline void
gbm_init_device(struct gbm *gbm)
{
    gbm->fd = open(gbm->params.path, O_RDWR);
    if (gbm->fd < 0)
        gbm_die("failed to open %s", gbm->params.path);

    gbm->dev = gbm_create_device(gbm->fd);
    if (!gbm->dev)
        gbm_die("failed to create gbm device");

    gbm->backend_name = gbm_device_get_backend_name(gbm->dev);

    if (gbm_device_get_fd(gbm->dev) != gbm->fd)
        gbm_die("unexpected fd change");
}

static inline void
gbm_init_formats(struct gbm *gbm)
{
    /* just some randomly chosen formats */
    const uint32_t all_formats[] = {
        DRM_FORMAT_BGR565,        DRM_FORMAT_RGB565,      DRM_FORMAT_R8,
        DRM_FORMAT_GR88,          DRM_FORMAT_BGR888,      DRM_FORMAT_RGB888,
        DRM_FORMAT_ABGR8888,      DRM_FORMAT_XBGR8888,    DRM_FORMAT_ARGB8888,
        DRM_FORMAT_XRGB8888,      DRM_FORMAT_ABGR2101010, DRM_FORMAT_XBGR2101010,
        DRM_FORMAT_ARGB2101010,   DRM_FORMAT_XRGB2101010, DRM_FORMAT_R16,
        DRM_FORMAT_ABGR16161616F, DRM_FORMAT_YUYV,        DRM_FORMAT_UYVY,
        DRM_FORMAT_NV12,          DRM_FORMAT_NV21,        DRM_FORMAT_YUV420,
        DRM_FORMAT_YVU420,        DRM_FORMAT_P010,        DRM_FORMAT_P016,
    };
    const uint32_t all_flags[] = {
        GBM_BO_USE_SCANOUT,
        GBM_BO_USE_CURSOR,
        GBM_BO_USE_RENDERING,
        GBM_BO_USE_WRITE,
        GBM_BO_USE_LINEAR,
        GBM_BO_USE_PROTECTED,
        GBM_BO_USE_FRONT_RENDERING,
#ifdef MINIGBM
        GBM_BO_USE_TEXTURING,
        GBM_BO_USE_CAMERA_WRITE,
        GBM_BO_USE_CAMERA_READ,
        GBM_BO_USE_SW_READ_OFTEN,
        GBM_BO_USE_SW_READ_RARELY,
        GBM_BO_USE_SW_WRITE_OFTEN,
        GBM_BO_USE_SW_WRITE_RARELY,
        GBM_BO_USE_HW_VIDEO_DECODER,
        GBM_BO_USE_HW_VIDEO_ENCODER,
        GBM_BO_USE_GPU_DATA_BUFFER,
        GBM_BO_USE_SENSOR_DIRECT_DATA,
#endif
    };
    const uint64_t all_modifiers[] = {
        /* DRM_FORMAT_MOD_VENDOR_NONE */
        DRM_FORMAT_MOD_LINEAR,
        /* DRM_FORMAT_MOD_VENDOR_INTEL */
        I915_FORMAT_MOD_X_TILED, I915_FORMAT_MOD_Y_TILED, I915_FORMAT_MOD_Yf_TILED,
        I915_FORMAT_MOD_Y_TILED_CCS, I915_FORMAT_MOD_Yf_TILED_CCS,
        I915_FORMAT_MOD_Y_TILED_GEN12_RC_CCS, I915_FORMAT_MOD_Y_TILED_GEN12_MC_CCS,
        I915_FORMAT_MOD_Y_TILED_GEN12_RC_CCS_CC, I915_FORMAT_MOD_4_TILED,
        I915_FORMAT_MOD_4_TILED_DG2_RC_CCS, I915_FORMAT_MOD_4_TILED_DG2_MC_CCS,
        I915_FORMAT_MOD_4_TILED_DG2_RC_CCS_CC, I915_FORMAT_MOD_4_TILED_MTL_RC_CCS,
        I915_FORMAT_MOD_4_TILED_MTL_MC_CCS, I915_FORMAT_MOD_4_TILED_MTL_RC_CCS_CC,
        /* XXX DRM_FORMAT_MOD_VENDOR_AMD */
        /* XXX DRM_FORMAT_MOD_VENDOR_NVIDIA */
        DRM_FORMAT_MOD_NVIDIA_TEGRA_TILED, DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_ONE_GOB,
        DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_TWO_GOB, DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_FOUR_GOB,
        DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_EIGHT_GOB,
        DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_SIXTEEN_GOB,
        DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_THIRTYTWO_GOB,
        /* DRM_FORMAT_MOD_VENDOR_SAMSUNG */
        DRM_FORMAT_MOD_SAMSUNG_64_32_TILE, DRM_FORMAT_MOD_SAMSUNG_16_16_TILE,
        /* DRM_FORMAT_MOD_VENDOR_QCOM */
        DRM_FORMAT_MOD_QCOM_COMPRESSED, DRM_FORMAT_MOD_QCOM_TILED3, DRM_FORMAT_MOD_QCOM_TILED2,
        /* XXX DRM_FORMAT_MOD_VENDOR_VIVANTE */
        DRM_FORMAT_MOD_VIVANTE_TILED, DRM_FORMAT_MOD_VIVANTE_SUPER_TILED,
        DRM_FORMAT_MOD_VIVANTE_SPLIT_TILED, DRM_FORMAT_MOD_VIVANTE_SPLIT_SUPER_TILED,
        /* XXX DRM_FORMAT_MOD_VENDOR_BROADCOM */
        DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED, DRM_FORMAT_MOD_BROADCOM_SAND32,
        DRM_FORMAT_MOD_BROADCOM_SAND64, DRM_FORMAT_MOD_BROADCOM_SAND128,
        DRM_FORMAT_MOD_BROADCOM_SAND256, DRM_FORMAT_MOD_BROADCOM_UIF,
        /* XXX DRM_FORMAT_MOD_VENDOR_ARM */
        DRM_FORMAT_MOD_ARM_16X16_BLOCK_U_INTERLEAVED,
        /* DRM_FORMAT_MOD_VENDOR_ALLWINNER */
        DRM_FORMAT_MOD_ALLWINNER_TILED,
        /* XXX DRM_FORMAT_MOD_VENDOR_AMLOGIC */
    };

    gbm->formats = calloc(ARRAY_SIZE(all_formats), sizeof(*gbm->formats));
    if (!gbm->formats)
        gbm_die("failed to alloc formats");

    for (uint32_t i = 0; i < ARRAY_SIZE(all_formats); i++) {
        const uint32_t fmt = all_formats[i];
        uint32_t flags = 0;

        for (uint32_t j = 0; j < ARRAY_SIZE(all_flags); j++) {
            const uint32_t f = all_flags[j];
            if (gbm_device_is_format_supported(gbm->dev, fmt, f))
                flags |= f;
        }

        if (flags) {
            struct gbm_format_info *info = &gbm->formats[gbm->format_count++];
            info->format = fmt;
            info->flags = flags;
        }
    }

    for (uint32_t i = 0; i < gbm->format_count; i++) {
        struct gbm_format_info *info = &gbm->formats[i];

        info->modifiers = calloc(ARRAY_SIZE(all_modifiers), sizeof(*info->modifiers));
        if (!info->modifiers)
            gbm_die("failed to alloc formats");

        for (uint32_t j = 0; j < ARRAY_SIZE(all_modifiers); j++) {
            const uint64_t mod = all_modifiers[j];
#ifdef MINIGBM
            struct gbm_bo *bo =
                gbm_bo_create_with_modifiers2(gbm->dev, 8, 8, info->format, &mod, 1, 0);
            if (bo) {
                if (gbm_bo_get_modifier(bo) == mod)
                    info->modifiers[info->modifier_count++] = mod;
                gbm_bo_destroy(bo);
            }
#else
            const int count =
                gbm_device_get_format_modifier_plane_count(gbm->dev, info->format, mod);
            if (count >= 0) {
                if (count == 0)
                    gbm_die("unexpected plane count 0");
                info->modifiers[info->modifier_count++] = mod;
            }
#endif
        }
    }
}

static inline void
gbm_init(struct gbm *gbm, const struct gbm_init_params *params)
{
    memset(gbm, 0, sizeof(*gbm));
    gbm->params = *params;
    gbm->fd = -1;

    gbm_init_device(gbm);
    gbm_init_formats(gbm);
}

static inline void
gbm_cleanup(struct gbm *gbm)
{
    for (uint32_t i = 0; i < gbm->format_count; i++)
        free(gbm->formats[i].modifiers);
    free(gbm->formats);

    gbm_device_destroy(gbm->dev);
    close(gbm->fd);
}

static inline void
gbm_free_bo_info(struct gbm_bo *bo, void *info)
{
    free(info);
}

static void
gbm_validate_bo_info(struct gbm *gbm,
                     struct gbm_bo *bo,
                     uint32_t width,
                     uint32_t height,
                     uint32_t format,
                     const uint64_t *modifiers,
                     uint32_t modifier_count)
{
    const struct gbm_bo_info *info = gbm_bo_get_user_data(bo);

    /* require the use of drm formats */
    switch (format) {
    case GBM_BO_FORMAT_XRGB8888:
    case GBM_BO_FORMAT_ARGB8888:
        gbm_die("invalid format %d", format);
        break;
    default:
        break;
    }

    if (info->width != width)
        gbm_die("unexpected width change");
    if (info->height != height)
        gbm_die("unexpected height change");
    if (info->format != format)
        gbm_die("unexpected format change");

    if (modifier_count) {
        bool found = false;
        for (uint32_t i = 0; i < modifier_count; i++) {
            if (modifiers[i] == info->modifier) {
                found = true;
                break;
            }
        }

        if (!found)
            gbm_die("unexpected modifier change");
        if (info->modifier == DRM_FORMAT_MOD_INVALID)
            gbm_die("unexpected invalid modifier");
    }
}

static inline void
gbm_init_bo_info(struct gbm *gbm, struct gbm_bo *bo)
{
    struct gbm_bo_info *info = calloc(1, sizeof(*info));
    if (!info)
        gbm_die("failed to alloc bo info");

    info->width = gbm_bo_get_width(bo);
    info->height = gbm_bo_get_height(bo);
    info->format = gbm_bo_get_format(bo);
    info->modifier = gbm_bo_get_modifier(bo);
    info->bpp = gbm_bo_get_bpp(bo);

    info->plane_count = gbm_bo_get_plane_count(bo);
    if (info->plane_count > GBM_MAX_PLANES)
        gbm_die("unexpected plane count");
    for (uint32_t i = 0; i < info->plane_count; i++) {
        info->offsets[i] = gbm_bo_get_offset(bo, i);
        info->strides[i] = gbm_bo_get_stride_for_plane(bo, i);
    }

    if (gbm_bo_get_device(bo) != gbm->dev)
        gbm_die("unexpceted dev change");
    if (info->strides[0] != gbm_bo_get_stride(bo))
        gbm_die("unexpceted stride change");
    if (gbm_bo_get_handle(bo).s32 != gbm_bo_get_handle_for_plane(bo, 0).s32)
        gbm_die("unexpceted handle change");
#ifndef MINIGBM
    /* minigbm always returns 0 for gbm_device_get_format_modifier_plane_count */
    if (info->plane_count != (uint32_t)gbm_device_get_format_modifier_plane_count(
                                 gbm->dev, info->format, info->modifier))
        gbm_die("unexpceted plane count change");
#endif

    gbm_bo_set_user_data(bo, info, gbm_free_bo_info);
}

static inline struct gbm_bo *
gbm_create_bo(struct gbm *gbm,
              uint32_t width,
              uint32_t height,
              uint32_t format,
              const uint64_t *modifiers,
              uint32_t modifier_count,
              uint32_t flags)
{
#ifdef MINIGBM
    struct gbm_bo *bo;
    if (modifier_count) {
        /* minigbm does not allow flags to be specified */
        bo = gbm_bo_create_with_modifiers2(gbm->dev, width, height, format, modifiers,
                                           modifier_count, 0);

        /* minigbm falls back to DRM_FORMAT_MOD_LINEAR automatically */
        if (bo) {
            bool found = false;
            const uint64_t mod = gbm_bo_get_modifier(bo);
            for (uint32_t i = 0; i < modifier_count; i++) {
                if (modifiers[i] == mod) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                gbm_bo_destroy(bo);
                bo = NULL;
            }
        }
    } else {
        bo = gbm_bo_create(gbm->dev, width, height, format, flags);
    }
#else
    /* when there is no modifier, this is the same as gbm_bo_create; when flags is
     * GBM_BO_USE_SCANOUT, this is the same as gbm_bo_create_with_modifiers
     */
    struct gbm_bo *bo = gbm_bo_create_with_modifiers2(gbm->dev, width, height, format, modifiers,
                                                      modifier_count, flags);
#endif
    if (!bo) {
        gbm_die("failed to alloc bo: size %dx%d, format %.*s, modifier count %d, flags 0x%x",
                width, height, 4, (const char *)&format, modifier_count, flags);
    }

    gbm_init_bo_info(gbm, bo);
    gbm_validate_bo_info(gbm, bo, width, height, format, modifiers, modifier_count);

    return bo;
}

static inline struct gbm_bo *
gbm_create_bo_from_dmabuf(struct gbm *gbm,
                          const struct gbm_import_fd_modifier_data *data,
                          uint32_t flags)
{
    struct gbm_bo *bo = gbm_bo_import(gbm->dev, GBM_BO_IMPORT_FD_MODIFIER, (void *)data, flags);
    if (!bo) {
        gbm_die("failed to import bo: size %dx%d, format %.*s, modifier 0x%" PRIx64
                ", flags 0x%x",
                data->width, data->height, 4, (const char *)&data->format, data->modifier, flags);
    }

    gbm_init_bo_info(gbm, bo);
    gbm_validate_bo_info(gbm, bo, data->width, data->height, data->format, &data->modifier, 1);

    const struct gbm_bo_info *info = gbm_bo_get_user_data(bo);
    if (info->plane_count != data->num_fds)
        gbm_die("unexpected plane count change");
    for (uint32_t i = 0; i < info->plane_count; i++) {
        if (info->offsets[i] != (uint32_t)data->offsets[i])
            gbm_die("unexpected plane offset change");
        if (info->strides[i] != (uint32_t)data->strides[i])
            gbm_die("unexpected plane stride change");
    }

    return bo;
}

static inline void
gbm_destroy_bo(struct gbm *gbm, struct gbm_bo *bo)
{
    gbm_bo_destroy(bo);
}

static inline void
gbm_export_bo(struct gbm *gbm, struct gbm_bo *bo, struct gbm_import_fd_modifier_data *data)
{
    const struct gbm_bo_info *info = gbm_bo_get_user_data(bo);
    data->width = info->width;
    data->height = info->height;
    data->format = info->format;
    data->modifier = info->modifier;

    data->num_fds = info->plane_count;
    for (uint32_t i = 0; i < info->plane_count; i++) {
        data->fds[i] = gbm_bo_get_fd_for_plane(bo, i);
        if (data->fds[i] < 0)
            gbm_die("failed to export plane fd");

        data->strides[i] = info->strides[i];
        data->offsets[i] = info->offsets[i];
    }

    const int fd = gbm_bo_get_fd(bo);
    struct stat s1;
    struct stat s2;
    if (fstat(data->fds[0], &s1) || fstat(fd, &s2))
        gbm_die("failed to stat exported fd");
    if (s1.st_ino != s2.st_ino)
        gbm_die("unexpected dmabuf inode change");
    close(fd);
}

static inline void *
gbm_map_bo(struct gbm *gbm, struct gbm_bo *bo, uint32_t flags, uint32_t *stride)
{
    struct gbm_bo_info *info = gbm_bo_get_user_data(bo);
    if (info->map_data)
        gbm_die("recursive mapping");

    void *ptr = gbm_bo_map(bo, 0, 0, info->width, info->height, flags, stride, &info->map_data);
    if (!ptr)
        gbm_die("failed to map bo");

    return ptr;
}

static inline void
gbm_unmap_bo(struct gbm *gbm, struct gbm_bo *bo)
{
    struct gbm_bo_info *info = gbm_bo_get_user_data(bo);
    if (!info->map_data)
        gbm_die("bo is not mapped");

    gbm_bo_unmap(bo, info->map_data);

    info->map_data = NULL;
}

#endif /* GBMUTIL_H */
