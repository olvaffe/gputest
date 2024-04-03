/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef DRMUTIL_H
#define DRMUTIL_H

#include "util.h"

#include <fcntl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

struct drm_init_params {
    int unused;
};

struct drm_properties {
    drmModePropertyPtr *props;
    uint64_t *values;
    uint32_t count;
};

struct drm_fb {
    uint32_t id;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint64_t modifier;

    uint32_t handles[4];
    uint32_t offsets[4];
    uint32_t pitches[4];
    uint32_t plane_count;

    struct drm_properties *properties;
};

struct drm_plane {
    uint32_t id;
    uint32_t *formats;
    uint32_t format_count;
    uint32_t possible_crtcs;

    uint32_t fb_id;
    uint32_t crtc_id;

    struct drm_properties *properties;
};

struct drm_crtc {
    uint32_t id;
    uint32_t gamma_size;

    bool mode_valid;
    struct drm_mode_modeinfo mode;
    uint32_t x;
    uint32_t y;
    uint64_t seq;
    uint64_t ns;

    struct drm_properties *properties;
};

struct drm_connector {
    uint32_t id;
    uint32_t type;
    uint32_t type_id;
    uint32_t width_mm;
    uint32_t height_mm;
    uint32_t possible_crtcs;

    uint32_t crtc_id;
    bool connected;

    struct drm_properties *properties;
};

struct drm {
    struct drm_init_params params;
    int ret;

    drmDevicePtr *devices;
    uint32_t device_count;

    int fd;
    int node_type;
    bool master;
    drmVersionPtr version;
    uint64_t caps[64];
    uint64_t client_caps[64];

    uint32_t max_width;
    uint32_t max_height;
    uint32_t min_width;
    uint32_t min_height;

    struct drm_fb *active_fbs;
    uint32_t active_fb_count;

    struct drm_plane *planes;
    uint32_t plane_count;

    struct drm_crtc *crtcs;
    uint32_t crtc_count;

    struct drm_connector *connectors;
    uint32_t connector_count;
};

static inline void PRINTFLIKE(1, 2) drm_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_logv("DRM", format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(1, 2) NORETURN drm_die(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_diev("DRM", format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(2, 3) drm_check(const struct drm *drm, const char *format, ...)
{
    if (drm->ret >= 0)
        return;

    va_list ap;
    va_start(ap, format);
    u_diev("DRM", format, ap);
    va_end(ap);
}

static inline void
drm_init_devices(struct drm *drm)
{
    drm->ret = drmGetDevices2(DRM_DEVICE_GET_PCI_REVISION, NULL, 0);
    drm_check(drm, "failed to get device count");

    const uint32_t count = drm->ret;
    drm->devices = malloc(sizeof(*drm->devices) * count);
    if (!drm->devices)
        drm_check(drm, "failed to alloc devices");

    drm->ret = drmGetDevices2(DRM_DEVICE_GET_PCI_REVISION, drm->devices, count);
    drm_check(drm, "failed to get devices");

    drm->device_count = drm->ret;
}

static inline void
drm_init(struct drm *drm, const struct drm_init_params *params)
{
    memset(drm, 0, sizeof(*drm));
    drm->fd = -1;

    if (params)
        drm->params = *params;

    drm_init_devices(drm);
}

static inline void
drm_cleanup(struct drm *drm)
{
    drmFreeDevices(drm->devices, drm->device_count);
}

static inline void
drm_open(struct drm *drm, uint32_t idx, int node_type)
{
    drmDevicePtr dev = drm->devices[idx];
    if (!(dev->available_nodes & (1 << node_type)))
        drm_die("bad node type");

    drm->fd = open(dev->nodes[node_type], O_RDWR);
    if (drm->fd < 0)
        drm_die("failed to open %s", dev->nodes[node_type]);

    drm->node_type = node_type;
    drm->master = drmIsMaster(drm->fd);

    drm->version = drmGetVersion(drm->fd);
    if (!drm->version)
        drm_die("failed to get version");

    const uint64_t cap_keys[] = {
        DRM_CAP_DUMB_BUFFER,
        DRM_CAP_VBLANK_HIGH_CRTC,
        DRM_CAP_DUMB_PREFERRED_DEPTH,
        DRM_CAP_DUMB_PREFER_SHADOW,
        DRM_CAP_PRIME,
        DRM_CAP_TIMESTAMP_MONOTONIC,
        DRM_CAP_ASYNC_PAGE_FLIP,
        DRM_CAP_CURSOR_WIDTH,
        DRM_CAP_CURSOR_HEIGHT,
        DRM_CAP_ADDFB2_MODIFIERS,
        DRM_CAP_PAGE_FLIP_TARGET,
        DRM_CAP_CRTC_IN_VBLANK_EVENT,
        DRM_CAP_SYNCOBJ,
        DRM_CAP_SYNCOBJ_TIMELINE,
        DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP,
    };
    for (uint32_t i = 0; i < ARRAY_SIZE(cap_keys); i++) {
        const uint64_t key = cap_keys[i];
        assert(key < ARRAY_SIZE(drm->caps));
        drm->ret = drmGetCap(drm->fd, key, &drm->caps[key]);
        if (drm->ret < 0)
            drm->caps[key] = 0;
    }

    if (node_type == DRM_NODE_PRIMARY) {
        const uint64_t client_cap_keys[] = { DRM_CLIENT_CAP_STEREO_3D,
                                             DRM_CLIENT_CAP_UNIVERSAL_PLANES,
                                             DRM_CLIENT_CAP_ATOMIC,
                                             DRM_CLIENT_CAP_ASPECT_RATIO,
                                             DRM_CLIENT_CAP_WRITEBACK_CONNECTORS,
                                             DRM_CLIENT_CAP_CURSOR_PLANE_HOTSPOT };
        for (uint32_t i = 0; i < ARRAY_SIZE(client_cap_keys); i++) {
            const uint64_t key = client_cap_keys[i];
            const uint64_t val = 1;
            assert(key < ARRAY_SIZE(drm->client_caps));
            drm->ret = drmSetClientCap(drm->fd, key, val);
            if (!drm->ret)
                drm->client_caps[key] = val;
        }
    }
}

static inline void
drm_close(struct drm *drm)
{
    for (uint32_t i = 0; i < drm->connector_count; i++)
        free(drm->connectors[i].properties);
    free(drm->connectors);

    for (uint32_t i = 0; i < drm->crtc_count; i++)
        free(drm->crtcs[i].properties);
    free(drm->crtcs);

    for (uint32_t i = 0; i < drm->plane_count; i++) {
        free(drm->planes[i].formats);
        free(drm->planes[i].properties);
    }
    free(drm->planes);

    for (uint32_t i = 0; i < drm->active_fb_count; i++) {
        struct drm_fb *fb = &drm->active_fbs[i];

        for (uint32_t j = 0; j < fb->plane_count; j++) {
            if (!fb->handles[j])
                continue;

            drmCloseBufferHandle(drm->fd, fb->handles[j]);
            for (uint32_t k = j + 1; k < fb->plane_count; k++) {
                if (fb->handles[k] == fb->handles[j])
                    fb->handles[k] = 0;
            }
        }

        free(fb->properties);
    }
    free(drm->active_fbs);

    memset(drm->client_caps, 0, sizeof(drm->client_caps));
    memset(drm->caps, 0, sizeof(drm->caps));

    drmFreeVersion(drm->version);
    drm->version = NULL;

    drm->master = false;
    drm->node_type = DRM_NODE_MAX;

    close(drm->fd);
    drm->fd = -1;
}

static inline struct drm_properties *
drm_scan_resource_properties(struct drm *drm, uint32_t res_id)
{
    drmModeObjectPropertiesPtr src =
        drmModeObjectGetProperties(drm->fd, res_id, DRM_MODE_OBJECT_ANY);
    if (!src)
        return NULL;

    struct drm_properties *dst =
        malloc(sizeof(*dst) + (sizeof(*dst->props) + sizeof(*dst->values)) * src->count_props);
    if (!dst)
        drm_die("failed to alloc props");
    dst->props = (void *)&dst[1];
    dst->values = (void *)&dst->props[src->count_props];
    dst->count = src->count_props;

    for (uint32_t i = 0; i < src->count_props; i++) {
        dst->props[i] = drmModeGetProperty(drm->fd, src->props[i]);
        dst->values[i] = src->prop_values[i];
    }

    drmModeFreeObjectProperties(src);

    return dst;
}

static inline void
drm_scan_resources(struct drm *drm)
{
    drmModeResPtr res = drmModeGetResources(drm->fd);
    drmModePlaneResPtr plane_res = drmModeGetPlaneResources(drm->fd);
    if (!res || !plane_res)
        drm_die("failed to get resources");

    drm->max_width = res->max_width;
    drm->max_height = res->max_height;
    drm->min_width = res->min_width;
    drm->min_height = res->min_height;

    if (res->count_fbs)
        drm_die("unexpected fb count");
    drm->active_fbs = calloc(plane_res->count_planes, sizeof(*drm->active_fbs));
    if (!drm->active_fbs)
        drm_die("failed to alloc fbs");

    drm->plane_count = plane_res->count_planes;
    drm->planes = calloc(plane_res->count_planes, sizeof(*drm->planes));
    if (!drm->planes)
        drm_die("failed to alloc planes");
    for (uint32_t i = 0; i < plane_res->count_planes; i++) {
        const uint32_t res_id = plane_res->planes[i];
        struct drm_plane *dst = &drm->planes[i];
        drmModePlanePtr src = drmModeGetPlane(drm->fd, res_id);

        dst->id = src->plane_id;

        static_assert(sizeof(*dst->formats) == sizeof(*src->formats), "");
        const size_t formats_size = sizeof(*src->formats) * src->count_formats;
        dst->formats = malloc(formats_size);
        memcpy(dst->formats, src->formats, formats_size);
        dst->format_count = src->count_formats;
        dst->possible_crtcs = src->possible_crtcs;

        dst->fb_id = src->fb_id;
        dst->crtc_id = src->crtc_id;

        if (src->crtc_x || src->crtc_y || src->x || src->y)
            drm_die("plane x/y is unexpectedly initialized by libdrm");
        if (src->gamma_size)
            drm_die("plane gamma is unexpectedly initialized by kernel");

        drmModeFreePlane(src);

        dst->properties = drm_scan_resource_properties(drm, res_id);

        /* count unique fb ids */
        if (dst->fb_id) {
            bool found = false;
            for (uint32_t i = 0; i < drm->active_fb_count; i++) {
                if (drm->active_fbs[i].id == dst->fb_id) {
                    found = true;
                    break;
                }
            }
            if (!found)
                drm->active_fbs[drm->active_fb_count++].id = dst->fb_id;
        }
    }

    for (uint32_t i = 0; i < drm->active_fb_count; i++) {
        struct drm_fb *dst = &drm->active_fbs[i];
        const uint32_t res_id = dst->id;
        drmModeFB2Ptr src = drmModeGetFB2(drm->fd, res_id);

        dst->width = src->width;
        dst->height = src->height;
        dst->format = src->pixel_format;
        dst->modifier =
            src->flags & DRM_MODE_FB_MODIFIERS ? src->modifier : DRM_FORMAT_MOD_INVALID;

        for (uint32_t j = 0; j < ARRAY_SIZE(src->pitches); j++) {
            if (src->pitches[j])
                dst->plane_count++;
        }
        static_assert(sizeof(dst->handles) == sizeof(src->handles), "");
        static_assert(sizeof(dst->offsets) == sizeof(src->offsets), "");
        static_assert(sizeof(dst->pitches) == sizeof(src->pitches), "");
        memcpy(dst->handles, src->handles, sizeof(*dst->handles) * dst->plane_count);
        memcpy(dst->offsets, src->offsets, sizeof(*dst->offsets) * dst->plane_count);
        memcpy(dst->pitches, src->pitches, sizeof(*dst->pitches) * dst->plane_count);

        drmModeFreeFB2(src);

        dst->properties = drm_scan_resource_properties(drm, res_id);
    }

    drm->crtc_count = res->count_crtcs;
    drm->crtcs = calloc(res->count_crtcs, sizeof(*drm->crtcs));
    if (!drm->crtcs)
        drm_die("failed to alloc crtcs");
    for (int i = 0; i < res->count_crtcs; i++) {
        const uint32_t res_id = res->crtcs[i];
        struct drm_crtc *dst = &drm->crtcs[i];
        drmModeCrtcPtr src = drmModeGetCrtc(drm->fd, res_id);

        dst->id = src->crtc_id;
        dst->gamma_size = src->gamma_size;

        dst->mode_valid = src->mode_valid;
        if (src->mode_valid) {
            static_assert(sizeof(dst->mode) == sizeof(src->mode), "");
            memcpy(&dst->mode, &src->mode, sizeof(src->mode));
        }
        dst->x = src->x;
        dst->y = src->y;

        drmModeFreeCrtc(src);

        drmCrtcGetSequence(drm->fd, dst->id, &dst->seq, &dst->ns);

        dst->properties = drm_scan_resource_properties(drm, res_id);
    }

    drmModeEncoderPtr *encoders = calloc(res->count_encoders, sizeof(*encoders));
    if (!encoders)
        drm_die("failed to alloc encoders");
    for (int i = 0; i < res->count_encoders; i++) {
        const uint32_t res_id = res->encoders[i];
        encoders[i] = drmModeGetEncoder(drm->fd, res_id);
    }

    drm->connector_count = res->count_connectors;
    drm->connectors = calloc(res->count_connectors, sizeof(*drm->connectors));
    if (!drm->connectors)
        drm_die("failed to alloc connectors");
    for (int i = 0; i < res->count_connectors; i++) {
        const uint32_t res_id = res->connectors[i];
        struct drm_connector *dst = &drm->connectors[i];
        drmModeConnectorPtr src = drmModeGetConnector(drm->fd, res_id);

        dst->id = src->connector_id;
        dst->type = src->connector_type;
        dst->type_id = src->connector_type_id;
        dst->width_mm = src->mmWidth;
        dst->height_mm = src->mmHeight;

        for (int j = 0; j < res->count_encoders; j++) {
            const drmModeEncoderPtr encoder = encoders[j];
            dst->possible_crtcs |= encoder->possible_crtcs;

            if (src->encoder_id == encoder->encoder_id)
                dst->crtc_id = encoder->crtc_id;
        }

        dst->connected = src->connection == DRM_MODE_CONNECTED;

        drmModeFreeConnector(src);

        dst->properties = drm_scan_resource_properties(drm, res_id);
    }

    for (int i = 0; i < res->count_encoders; i++)
        drmModeFreeEncoder(encoders[i]);
    free(encoders);

    drmModeFreeResources(res);
    drmModeFreePlaneResources(plane_res);
}

static inline void
drm_dump_property(struct drm *drm, const drmModePropertyPtr prop, uint64_t val, const char *indent)
{
    const bool immutable = prop->flags & DRM_MODE_PROP_IMMUTABLE;
    const bool atomic = prop->flags & DRM_MODE_PROP_ATOMIC;
    const uint32_t type = drmModeGetPropertyType(prop);

    char val_str[DRM_PROP_NAME_LEN * 3] = "invalid";
    int cur = 0;
    switch (type) {
    case DRM_MODE_PROP_RANGE:
        snprintf(val_str, sizeof(val_str), "val %" PRIu64, val);
        break;
    case DRM_MODE_PROP_ENUM:
        for (int i = 0; i < prop->count_enums; i++) {
            const struct drm_mode_property_enum *p = &prop->enums[i];
            if (p->value == val) {
                snprintf(val_str, sizeof(val_str), "val %" PRIi64 " (%s)", val, p->name);
                break;
            }
        }
        break;
    case DRM_MODE_PROP_BLOB:
        snprintf(val_str, sizeof(val_str), "blob %d", (uint32_t)val);
        break;
    case DRM_MODE_PROP_BITMASK:
        cur = snprintf(val_str, sizeof(val_str), "val 0x%" PRIx64, val);
        if (val) {
            cur += snprintf(val_str + cur, sizeof(val_str) - cur, " (");

            for (int i = 0; i < prop->count_enums; i++) {
                const struct drm_mode_property_enum *p = &prop->enums[i];
                if (val & (1ull << p->value))
                    cur += snprintf(val_str + cur, sizeof(val_str) - cur, "%s|", p->name);
            }
            cur -= 1;

            cur += snprintf(val_str + cur, sizeof(val_str) - cur, ")");
        }
        break;
    case DRM_MODE_PROP_OBJECT:
        snprintf(val_str, sizeof(val_str), "obj %d", (uint32_t)val);
        break;
    case DRM_MODE_PROP_SIGNED_RANGE:
        snprintf(val_str, sizeof(val_str), "val %" PRIi64, val);
        break;
    default:
        break;
    }

    drm_log("%s%s%s \"%s\": %s", indent, immutable ? "immutable " : "",
            atomic ? "atomic" : "prop", prop->name, val_str);
}

static inline void
drm_dump_properties(struct drm *drm, const struct drm_properties *props, const char *indent)
{
    for (uint32_t i = 0; i < props->count; i++)
        drm_dump_property(drm, props->props[i], props->values[i], indent);
}

static inline void
drm_dump_plane_formats(struct drm *drm, const struct drm_plane *plane, const char *indent)
{
    const struct drm_properties *props = plane->properties;

    drmModePropertyBlobPtr in_formats_blob = NULL;
    for (uint32_t i = 0; i < props->count; i++) {
        const drmModePropertyPtr prop = props->props[i];
        if (drmModeGetPropertyType(prop) != DRM_MODE_PROP_BLOB ||
            strcmp(prop->name, "IN_FORMATS"))
            continue;

        const uint32_t blob_id = props->values[i];
        in_formats_blob = drmModeGetPropertyBlob(drm->fd, blob_id);
        break;
    }

    if (in_formats_blob) {
        drmModeFormatModifierIterator iter;
        memset(&iter, 0, sizeof(iter));
        while (drmModeFormatModifierBlobIterNext(in_formats_blob, &iter)) {
            drm_log("%sformat '%.*s': 0x%" PRIx64, indent, 4, (const char *)&iter.fmt, iter.mod);
        }
        drmModeFreePropertyBlob(in_formats_blob);
    } else {
        for (uint32_t j = 0; j < plane->format_count; j++)
            drm_log("%sformat '%.*s'", indent, 4, (const char *)&plane->formats[j]);
    }
}

#endif /* DRMUTIL_H */
