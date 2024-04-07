/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "drmutil.h"
#include "vautil.h"

static void
info_subpics(const struct va *va)
{
    va_log("subpicture formats:");
    for (unsigned int i = 0; i < va->subpic_count; i++) {
        const VAImageFormat *fmt = &va->subpic_formats[i];
        const char *fourcc = (const char *)&fmt->fourcc;
        unsigned flags = va->subpic_flags[i];

        va_log("  %c%c%c%c: 0x%x", fourcc[0], fourcc[1], fourcc[2], fourcc[3], flags);
    }
}

static void
info_images(const struct va *va)
{
    va_log("image formats:");
    for (unsigned int i = 0; i < va->img_count; i++) {
        const VAImageFormat *fmt = &va->img_formats[i];
        const char *fourcc = (const char *)&fmt->fourcc;

        va_log("  %c%c%c%c", fourcc[0], fourcc[1], fourcc[2], fourcc[3]);
    }
}

static void
info_surface_attr(struct va *va, const VASurfaceAttrib *attr)
{
    if (attr->flags == VA_SURFACE_ATTRIB_NOT_SUPPORTED)
        return;

    const char *name;
    switch (attr->type) {
#define CASE(a)                                                                                  \
    case VASurfaceAttrib##a:                                                                     \
        name = #a;                                                                               \
        break
        CASE(PixelFormat);
        CASE(MinWidth);
        CASE(MaxWidth);
        CASE(MinHeight);
        CASE(MaxHeight);
        CASE(MemoryType);
        CASE(ExternalBufferDescriptor);
#undef CASE
    default:
        name = "Unknown";
        break;
    }

    const char *type;
    switch (attr->value.type) {
    case VAGenericValueTypeInteger:
        type = "integer";
        break;
    case VAGenericValueTypeFloat:
        type = "float";
        break;
    case VAGenericValueTypePointer:
        type = "pointer";
        break;
    case VAGenericValueTypeFunc:
        type = "func";
        break;
    default:
        type = "unknown";
        break;
    }

    va_log("  %s: type %s, flags 0x%x", name, type, attr->flags);

    if (attr->flags & VA_SURFACE_ATTRIB_GETTABLE) {
        switch (attr->type) {
        case VASurfaceAttribPixelFormat:
            va_log("    fourcc '%.*s'", 4, (const char *)&attr->value.value.i);
            break;
        case VASurfaceAttribMemoryType:
            for (int i = 0; i < 32; i++) {
                const uint32_t mem_type = 1u << i;
                if (attr->value.value.i & mem_type) {
                    const char *str;
                    switch (mem_type) {
                    case VA_SURFACE_ATTRIB_MEM_TYPE_VA:
                        str = "VA";
                        break;
                    case VA_SURFACE_ATTRIB_MEM_TYPE_V4L2:
                        str = "V4L2";
                        break;
                    case VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR:
                        str = "USER_PTR";
                        break;
                    case VA_SURFACE_ATTRIB_MEM_TYPE_KERNEL_DRM:
                        str = "KERNEL_DRM";
                        break;
                    case VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME:
                        str = "DRM_PRIME";
                        break;
                    case VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2:
                        str = "DRM_PRIME_2";
                        break;
                    default:
                        str = NULL;
                        break;
                    }

                    if (str)
                        va_log("    %s", str);
                    else
                        va_log("    0x%x", mem_type);
                }
            }
            break;
        default:
            switch (attr->value.type) {
            case VAGenericValueTypeInteger:
                va_log("    val %d", attr->value.value.i);
                break;
            case VAGenericValueTypeFloat:
                va_log("    val %f", attr->value.value.f);
                break;
            default:
                break;
            }
        }
    }
}

static void
info_pair_default_surface(struct va *va, const struct va_pair *pair)
{
    VAConfigID config;
    va->status = vaCreateConfig(va->display, pair->profile, pair->entrypoint, NULL, 0, &config);
    va_check(va, "failed to create config");

    unsigned int count;
    va->status = vaQuerySurfaceAttributes(va->display, config, NULL, &count);
    va_check(va, "failed to query surface attr count");

    VASurfaceAttrib *attrs = malloc(sizeof(*attrs) * count);
    if (!attrs)
        va_die("failed to alloc surface attrs");
    va->status = vaQuerySurfaceAttributes(va->display, config, attrs, &count);
    if (va->status != VA_STATUS_SUCCESS)
        count = 0;

    vaDestroyConfig(va->display, config);

    for (unsigned int i = 0; i < count; i++) {
        const VASurfaceAttrib *attr = &attrs[i];
        info_surface_attr(va, attr);
    }

    free(attrs);
}

static void
info_pair_attr(const struct va *va, const struct va_pair *pair, const VAConfigAttrib *attr)
{
    if (attr->value == VA_ATTRIB_NOT_SUPPORTED)
        return;

    const char *name = vaConfigAttribTypeStr(attr->type) + 14;
    va_log("  %s: %d", name, attr->value);

    switch (attr->type) {
    case VAConfigAttribRTFormat:
        for (int i = 0; i < 32; i++) {
            const uint32_t fmt = 1u << i;
            if (attr->value & fmt) {
                switch (fmt) {
#define RT_CASE(f)                                                                               \
    case VA_RT_FORMAT_##f:                                                                       \
        va_log("    " #f);                                                                       \
        break
                    RT_CASE(YUV420);
                    RT_CASE(YUV422);
                    RT_CASE(YUV444);
                    RT_CASE(YUV411);
                    RT_CASE(YUV400);
                    RT_CASE(YUV420_10);
                    RT_CASE(YUV422_10);
                    RT_CASE(YUV444_10);
                    RT_CASE(YUV420_12);
                    RT_CASE(YUV422_12);
                    RT_CASE(YUV444_12);
                    RT_CASE(RGB16);
                    RT_CASE(RGB32);
                    RT_CASE(RGBP);
                    RT_CASE(RGB32_10);
                    RT_CASE(PROTECTED);
#undef RT_CASE
                default:
                    va_log("    0x%x", fmt);
                    break;
                }
            }
        }
        break;
    default:
        break;
    }
}

static void
info_pairs(struct va *va)
{
    for (int i = 0; i < va->pair_count; i++) {
        const struct va_pair *pair = &va->pairs[i];

        va_log("config (%s, %s) attrs:", vaProfileStr(pair->profile),
               vaEntrypointStr(pair->entrypoint));
        for (int j = 0; j < VAConfigAttribTypeMax; j++) {
            const VAConfigAttrib *attr = &pair->attrs[j];
            info_pair_attr(va, pair, attr);
        }

        va_log("config (%s, %s) default surface attrs:", vaProfileStr(pair->profile),
               vaEntrypointStr(pair->entrypoint));
        info_pair_default_surface(va, pair);
    }
}

static void
info_display(const struct va *va)
{
    va_log("version: %d.%d", va->major, va->minor);
    va_log("vendor: %s", va->vendor);
    va_log("display attrs:");

    for (int i = 0; i < va->attr_count; i++) {
        const VADisplayAttribute *attr = &va->attrs[i];

        assert(attr->flags);
        switch (attr->type) {
        case VADisplayAttribCopy:
            assert(attr->flags == VA_DISPLAY_ATTRIB_GETTABLE);
            va_log("  Copy: 0x%x", attr->value);
            break;
        case VADisplayPCIID:
            assert(attr->flags == VA_DISPLAY_ATTRIB_GETTABLE);
            va_log("  PCIID: 0x%04x:0x%04x", (attr->value >> 16) & 0xffff, attr->value & 0xffff);
            break;
        default:
            va_log("  type %d: min %d max %d val %d flags 0x%x", attr->type, attr->min_value,
                   attr->max_value, attr->value, attr->flags);
            break;
        }
    }
}

int
main(void)
{
    struct drm drm;
    drm_init(&drm, NULL);
    drm_open(&drm, 0, DRM_NODE_RENDER);

    const struct va_init_params params = {
        .drm_fd = drm.fd,
    };
    struct va va;
    va_init(&va, &params);

    info_display(&va);
    info_pairs(&va);
    info_images(&va);
    info_subpics(&va);

    va_cleanup(&va);

    drm_close(&drm);
    drm_cleanup(&drm);

    return 0;
}
