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

    bool replaced;
};

#define drm_log(format, ...) u_log("DRM", format __VA_OPT__(, ) __VA_ARGS__)
#define drm_die(format, ...) u_die("DRM", format __VA_OPT__(, ) __VA_ARGS__)

#define drm_check(drm, format, ...)                                                              \
    u_check("DRM", (drm)->ret >= 0, format __VA_OPT__(, ) __VA_ARGS__)

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
    if (!res || !plane_res) {
        drm_log("failed to get resources");
        return;
    }

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
drm_init_dumb_fb(struct drm *drm, struct drm_dumb *dumb)
{
    const uint32_t handles[4] = { dumb->handle };
    const uint32_t pitches[4] = { dumb->pitch };
    const uint32_t offsets[4] = { 0 };
    if (drmModeAddFB2WithModifiers(drm->fd, dumb->width, dumb->height, dumb->format, handles,
                                   pitches, offsets, NULL, &dumb->fb_id, 0))
        drm_die("failed to create fb");
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

    drm_init_dumb_fb(drm, dumb);

    return dumb;
}

static inline void
drm_destroy_dumb(struct drm *drm, struct drm_dumb *dumb)
{
    if (dumb->map)
        drm_die("mapped dumb");

    drmModeRmFB(drm->fd, dumb->fb_id);

    if (dumb->replaced)
        drmCloseBufferHandle(drm->fd, dumb->handle);
    else
        drmModeDestroyDumbBuffer(drm->fd, dumb->handle);

    free(dumb);
}

static inline void *
drm_map_dumb(struct drm *drm, struct drm_dumb *dumb)
{
    if (dumb->map)
        drm_die("nested dumb map");
    if (dumb->replaced)
        drm_die("failed to map replaced dumb");

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
drm_replace_dumb_storage(struct drm *drm, struct drm_dumb *dumb, uint32_t handle)
{
    if (dumb->map)
        drm_die("failed to replace mapped dumb");

    dumb->handle = handle;

    drmModeRmFB(drm->fd, dumb->fb_id);
    drm_init_dumb_fb(drm, dumb);

    dumb->replaced = true;
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

    return handle;
}

#endif /* DRMUTIL_H */
