/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef DRMUTIL_H
#define DRMUTIL_H

#include "util.h"

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

    struct drm_mode_modeinfo *modes;
    uint32_t mode_count;

    uint32_t crtc_id;
    bool connected;

    struct drm_properties *properties;
};

struct drm_modeset {
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

struct drm_file {
    int node_type;
    bool master;
    drmVersionPtr version;
    uint64_t caps[64];
    uint64_t client_caps[64];
};

struct drm {
    struct drm_init_params params;
    int ret;

    drmDevicePtr *devices;
    uint32_t device_count;

    int fd;
    struct drm_file file;
    struct drm_modeset modeset;

    drmModeAtomicReqPtr req;
};

struct drm_dumb {
    uint32_t width;
    uint32_t height;
    uint32_t format;

    uint32_t handle;
    uint32_t pitch;
    uint64_t size;

    uint32_t fb_id;

    void *map;
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
    if (!drm->ret)
        drm_die("no drm device");

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
    if (drm->req)
        drmModeAtomicFree(drm->req);

    drmFreeDevices(drm->devices, drm->device_count);
}

static inline void
drm_open(struct drm *drm, uint32_t idx, int node_type)
{
    struct drm_file *file = &drm->file;

    if (idx >= drm->device_count)
        drm_die("bad device index");

    drmDevicePtr dev = drm->devices[idx];
    if (!(dev->available_nodes & (1 << node_type)))
        drm_die("bad node type");

    drm->fd = open(dev->nodes[node_type], O_RDWR);
    if (drm->fd < 0)
        drm_die("failed to open %s", dev->nodes[node_type]);

    file->node_type = node_type;
    file->master = drmIsMaster(drm->fd);

    file->version = drmGetVersion(drm->fd);
    if (!file->version)
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
        assert(key < ARRAY_SIZE(file->caps));
        drm->ret = drmGetCap(drm->fd, key, &file->caps[key]);
        if (drm->ret < 0)
            file->caps[key] = 0;
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
            assert(key < ARRAY_SIZE(file->client_caps));
            drm->ret = drmSetClientCap(drm->fd, key, val);
            if (!drm->ret)
                file->client_caps[key] = val;
        }
    }
}

static inline void
drm_close(struct drm *drm)
{
    drmFreeVersion(drm->file.version);
    memset(&drm->file, 0, sizeof(drm->file));

    close(drm->fd);
    drm->fd = -1;
}

static inline void
drm_dump_file(struct drm *drm)
{
    const struct drm_file *file = &drm->file;

    drm_log("  fd node type: %s", file->node_type == DRM_NODE_PRIMARY ? "primary" : "render");
    drm_log("  fd master: %d", file->master);
    drm_log("  version: %d.%d.%d", file->version->version_major, file->version->version_minor,
            file->version->version_patchlevel);
    drm_log("    name: %s", file->version->name);
    drm_log("    date: %s", file->version->date);
    drm_log("    desc: %s", file->version->desc);
    drm_log("  caps:");
    drm_log("    dumb_buffer: %" PRIu64, file->caps[DRM_CAP_DUMB_BUFFER]);
    drm_log("    vblank_high_crtc: %" PRIu64, file->caps[DRM_CAP_VBLANK_HIGH_CRTC]);
    drm_log("    dumb_preferred_depth: %" PRIu64, file->caps[DRM_CAP_DUMB_PREFERRED_DEPTH]);
    drm_log("    dumb_prefer_shadow: %" PRIu64, file->caps[DRM_CAP_DUMB_PREFER_SHADOW]);
    drm_log("    prime: %" PRIu64, file->caps[DRM_CAP_PRIME]);
    drm_log("    timestamp_monotonic: %" PRIu64, file->caps[DRM_CAP_TIMESTAMP_MONOTONIC]);
    drm_log("    async_page_flip: %" PRIu64, file->caps[DRM_CAP_ASYNC_PAGE_FLIP]);
    drm_log("    cursor_width: %" PRIu64, file->caps[DRM_CAP_CURSOR_WIDTH]);
    drm_log("    cursor_height: %" PRIu64, file->caps[DRM_CAP_CURSOR_HEIGHT]);
    drm_log("    addfb2_modifiers: %" PRIu64, file->caps[DRM_CAP_ADDFB2_MODIFIERS]);
    drm_log("    page_flip_target: %" PRIu64, file->caps[DRM_CAP_PAGE_FLIP_TARGET]);
    drm_log("    crtc_in_vblank_event: %" PRIu64, file->caps[DRM_CAP_CRTC_IN_VBLANK_EVENT]);
    drm_log("    syncobj: %" PRIu64, file->caps[DRM_CAP_SYNCOBJ]);
    drm_log("    syncobj_timeline: %" PRIu64, file->caps[DRM_CAP_SYNCOBJ_TIMELINE]);
    drm_log("    atomic_async_page_flip: %" PRIu64, file->caps[DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP]);

    if (file->node_type == DRM_NODE_PRIMARY) {
        drm_log("  client caps:");
        drm_log("    stereo_3d: %" PRIu64, file->client_caps[DRM_CLIENT_CAP_STEREO_3D]);
        drm_log("    universal_planes: %" PRIu64,
                file->client_caps[DRM_CLIENT_CAP_UNIVERSAL_PLANES]);
        drm_log("    atomic: %" PRIu64, file->client_caps[DRM_CLIENT_CAP_ATOMIC]);
        drm_log("    aspect_ratio: %" PRIu64, file->client_caps[DRM_CLIENT_CAP_ASPECT_RATIO]);
        drm_log("    writeback_connectors: %" PRIu64,
                file->client_caps[DRM_CLIENT_CAP_WRITEBACK_CONNECTORS]);
        drm_log("    cursor_plane_hotspot: %" PRIu64,
                file->client_caps[DRM_CLIENT_CAP_CURSOR_PLANE_HOTSPOT]);
    }
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
    struct drm_modeset *modeset = &drm->modeset;

    drmModeResPtr res = drmModeGetResources(drm->fd);
    drmModePlaneResPtr plane_res = drmModeGetPlaneResources(drm->fd);
    if (!res || !plane_res)
        drm_die("failed to get resources");

    modeset->max_width = res->max_width;
    modeset->max_height = res->max_height;
    modeset->min_width = res->min_width;
    modeset->min_height = res->min_height;

    if (res->count_fbs)
        drm_die("unexpected fb count");
    modeset->active_fbs = calloc(plane_res->count_planes, sizeof(*modeset->active_fbs));
    if (!modeset->active_fbs)
        drm_die("failed to alloc fbs");

    modeset->plane_count = plane_res->count_planes;
    modeset->planes = calloc(plane_res->count_planes, sizeof(*modeset->planes));
    if (!modeset->planes)
        drm_die("failed to alloc planes");
    for (uint32_t i = 0; i < plane_res->count_planes; i++) {
        const uint32_t res_id = plane_res->planes[i];
        struct drm_plane *dst = &modeset->planes[i];
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
            for (uint32_t i = 0; i < modeset->active_fb_count; i++) {
                if (modeset->active_fbs[i].id == dst->fb_id) {
                    found = true;
                    break;
                }
            }
            if (!found)
                modeset->active_fbs[modeset->active_fb_count++].id = dst->fb_id;
        }
    }

    for (uint32_t i = 0; i < modeset->active_fb_count; i++) {
        struct drm_fb *dst = &modeset->active_fbs[i];
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

    modeset->crtc_count = res->count_crtcs;
    modeset->crtcs = calloc(res->count_crtcs, sizeof(*modeset->crtcs));
    if (!modeset->crtcs)
        drm_die("failed to alloc crtcs");
    for (int i = 0; i < res->count_crtcs; i++) {
        const uint32_t res_id = res->crtcs[i];
        struct drm_crtc *dst = &modeset->crtcs[i];
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

    modeset->connector_count = res->count_connectors;
    modeset->connectors = calloc(res->count_connectors, sizeof(*modeset->connectors));
    if (!modeset->connectors)
        drm_die("failed to alloc connectors");
    for (int i = 0; i < res->count_connectors; i++) {
        const uint32_t res_id = res->connectors[i];
        struct drm_connector *dst = &modeset->connectors[i];
        drmModeConnectorPtr src = drmModeGetConnector(drm->fd, res_id);

        dst->id = src->connector_id;
        dst->type = src->connector_type;
        dst->type_id = src->connector_type_id;
        dst->width_mm = src->mmWidth;
        dst->height_mm = src->mmHeight;

        static_assert(sizeof(*dst->modes) == sizeof(*src->modes), "");
        const size_t modes_size = sizeof(*src->modes) * src->count_modes;
        dst->modes = malloc(modes_size);
        if (!dst->modes)
            drm_die("failed to alloc modes");
        memcpy(dst->modes, src->modes, modes_size);
        dst->mode_count = src->count_modes;

        for (int j = 0; j < src->count_encoders; j++) {
            drmModeEncoderPtr encoder = NULL;
            for (int k = 0; k < res->count_encoders; k++) {
                if (encoders[k]->encoder_id == src->encoders[j]) {
                    encoder = encoders[k];
                    break;
                }
            }
            if (!encoder)
                drm_die("bad encoder");

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
drm_release_resources(struct drm *drm)
{
    struct drm_modeset *modeset = &drm->modeset;

    for (uint32_t i = 0; i < modeset->connector_count; i++)
        free(modeset->connectors[i].properties);
    free(modeset->connectors);

    for (uint32_t i = 0; i < modeset->crtc_count; i++)
        free(modeset->crtcs[i].properties);
    free(modeset->crtcs);

    for (uint32_t i = 0; i < modeset->plane_count; i++) {
        free(modeset->planes[i].formats);
        free(modeset->planes[i].properties);
    }
    free(modeset->planes);

    for (uint32_t i = 0; i < modeset->active_fb_count; i++) {
        struct drm_fb *fb = &modeset->active_fbs[i];

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
    free(modeset->active_fbs);

    memset(modeset, 0, sizeof(*modeset));
}

static inline void
drm_dump_device(struct drm *drm, uint32_t idx)
{
    const drmDevicePtr dev = drm->devices[idx];

    drm_log("device %d", idx);
    for (int i = 0; i < DRM_NODE_MAX; i++) {
        if (!(dev->available_nodes & (1 << i)))
            continue;
        drm_log("  node type %d: %s", i, dev->nodes[i]);
    }

    switch (dev->bustype) {
    case DRM_BUS_PCI:
        drm_log("  bus type: pci");
        drm_log("  bus info: %04x:%02x:%02x.%u", dev->businfo.pci->domain, dev->businfo.pci->bus,
                dev->businfo.pci->dev, dev->businfo.pci->func);
        drm_log("  dev info: %04x:%04x, revision %02x, subsystem %04x:%04x",
                dev->deviceinfo.pci->vendor_id, dev->deviceinfo.pci->device_id,
                dev->deviceinfo.pci->revision_id, dev->deviceinfo.pci->subvendor_id,
                dev->deviceinfo.pci->subdevice_id);
        break;
    default:
        drm_log("  bus type %d", dev->bustype);
        break;
    }
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

static inline void
drm_dump_modeset(struct drm *drm, bool dump_all)
{
    const struct drm_modeset *modeset = &drm->modeset;

    drm_log("  min size: %dx%d", modeset->min_width, modeset->min_height);
    drm_log("  max size: %dx%d", modeset->max_width, modeset->max_height);

    drm_log("  active fb count: %d", modeset->active_fb_count);
    for (uint32_t i = 0; i < modeset->active_fb_count; i++) {
        const struct drm_fb *fb = &modeset->active_fbs[i];
        drm_log("    active fb[%d]: id %d, size %dx%d, format '%.*s', modifier 0x%" PRIx64
                ", plane count %d",
                i, fb->id, fb->width, fb->height, 4, (const char *)&fb->format, fb->modifier,
                fb->plane_count);

        for (uint32_t j = 0; j < fb->plane_count; j++) {
            drm_log("      plane[%d]: handle %d, offset %d, pitch %d", j, fb->handles[j],
                    fb->offsets[j], fb->pitches[j]);
        }

        if (fb->properties)
            drm_dump_properties(drm, fb->properties, "      ");
    }

    drm_log("  plane count: %d", modeset->plane_count);
    for (uint32_t i = 0; i < modeset->plane_count; i++) {
        const struct drm_plane *plane = &modeset->planes[i];
        if (!plane->crtc_id && !dump_all)
            continue;

        drm_log("    plane[%d]: id %d, fb id %d, crtc id %d, mask 0x%x, format count %d", i,
                plane->id, plane->fb_id, plane->crtc_id, plane->possible_crtcs,
                plane->format_count);

        if (dump_all)
            drm_dump_plane_formats(drm, plane, "      ");

        if (plane->properties)
            drm_dump_properties(drm, plane->properties, "      ");
    }

    drm_log("  crtc count: %d", modeset->crtc_count);
    for (uint32_t i = 0; i < modeset->crtc_count; i++) {
        const struct drm_crtc *crtc = &modeset->crtcs[i];
        if (!crtc->mode_valid && !dump_all)
            continue;

        drm_log("    crtc[%d]: id %d, mode %s, offset %dx%d, seq %" PRIu64 ", ns %" PRIu64
                ", gamma %d",
                i, crtc->id, crtc->mode.name[0] != '\0' ? crtc->mode.name : "invalid", crtc->x,
                crtc->y, crtc->seq, crtc->ns, crtc->gamma_size);

        if (crtc->properties)
            drm_dump_properties(drm, crtc->properties, "      ");
    }

    drm_log("  connector count: %d", modeset->connector_count);
    for (uint32_t i = 0; i < modeset->connector_count; i++) {
        const struct drm_connector *connector = &modeset->connectors[i];
        if (!connector->crtc_id && !dump_all)
            continue;

        drm_log("    connector[%d]: id %d, crtc id %d, connected %d, type %s-%d, size %dx%d, "
                "mask 0x%x",
                i, connector->id, connector->crtc_id, connector->connected,
                drmModeGetConnectorTypeName(connector->type), connector->type_id,
                connector->width_mm, connector->height_mm, connector->possible_crtcs);

        for (uint32_t i = 0; i < connector->mode_count; i++) {
            const struct drm_mode_modeinfo *mode = &connector->modes[i];
            drm_log("      mode[%d]: %dx%d@%d%s", i, mode->hdisplay, mode->vdisplay,
                    mode->vrefresh, mode->type & DRM_MODE_TYPE_PREFERRED ? ", preferred" : "");
        }

        if (connector->properties)
            drm_dump_properties(drm, connector->properties, "      ");
    }
}

static inline struct drm_dumb *
drm_create_dumb(struct drm *drm, uint32_t width, uint32_t height, uint32_t format)
{
    struct drm_dumb *dumb = (struct drm_dumb *)calloc(1, sizeof(*dumb));
    if (!dumb)
        drm_die("failed to alloc dumb");

    dumb->width = width;
    dumb->height = height;
    dumb->format = format;

    const uint32_t bpp = u_drm_format_to_cpp(format) * 8;
    if (drmModeCreateDumbBuffer(drm->fd, width, height, bpp, 0, &dumb->handle, &dumb->pitch,
                                &dumb->size))
        drm_die("failed to create dumb");

    const uint32_t handles[4] = { dumb->handle };
    const uint32_t pitches[4] = { dumb->pitch };
    const uint32_t offsets[4] = { 0 };
    if (drmModeAddFB2WithModifiers(drm->fd, width, height, format, handles, pitches, offsets,
                                   NULL, &dumb->fb_id, 0))
        drm_die("failed to create fb");

    return dumb;
}

static inline void
drm_destroy_dumb(struct drm *drm, struct drm_dumb *dumb)
{
    if (dumb->map)
        drm_die("mapped dumb");

    drmModeRmFB(drm->fd, dumb->fb_id);
    drmModeDestroyDumbBuffer(drm->fd, dumb->handle);
    free(dumb);
}

static inline void *
drm_map_dumb(struct drm *drm, struct drm_dumb *dumb)
{
    if (dumb->map)
        drm_die("nested dumb map");

    uint64_t offset;
    if (drmModeMapDumbBuffer(drm->fd, dumb->handle, &offset))
        drm_die("failed to map dumb");

    dumb->map = mmap(NULL, dumb->size, PROT_READ | PROT_WRITE, MAP_SHARED, drm->fd, offset);
    if (dumb->map == MAP_FAILED)
        drm_die("failed to mmap dumb");

    return dumb->map;
}

static inline void
drm_unmap_dumb(struct drm *drm, struct drm_dumb *dumb)
{
    munmap(dumb->map, dumb->size);
    dumb->map = NULL;
}

static inline void
drm_reset_req(struct drm *drm)
{
    if (drm->req)
        drmModeAtomicFree(drm->req);

    drm->req = drmModeAtomicAlloc();
    if (!drm->req)
        drm_die("failed to alloc req");
}

static inline void
drm_add_property(struct drm *drm,
                 uint32_t obj_id,
                 const struct drm_properties *props,
                 const char *name,
                 uint64_t val)
{
    uint32_t prop_id = 0;
    for (uint32_t i = 0; i < props->count; i++) {
        const drmModePropertyPtr prop = props->props[i];
        if (!strcmp(prop->name, name)) {
            prop_id = prop->prop_id;
            break;
        }
    }
    if (!prop_id)
        drm_die("failed to find property %s", name);

    if (drmModeAtomicAddProperty(drm->req, obj_id, prop_id, val) < 0)
        drm_die("failed to add property");
}

static inline void
drm_commit(struct drm *drm)
{
    if (drmModeAtomicCommit(drm->fd, drm->req, 0, NULL))
        drm_die("failed to commit");
}

static inline int
drm_prime_export(struct drm *drm, uint32_t handle)
{
    int fd;
    if (drmPrimeHandleToFD(drm->fd, handle, DRM_RDWR | DRM_CLOEXEC, &fd))
        drm_die("failed to export");

    return fd;
}

static inline uint32_t
drm_prime_import(struct drm *drm, int fd)
{
    uint32_t handle;
    if (drmPrimeFDToHandle(drm->fd, fd, &handle))
        drm_die("failed to import");

    /* take ownership */
    close(fd);

    return handle;
}

#endif /* DRMUTIL_H */
