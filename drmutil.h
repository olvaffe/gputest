/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef DRMUTIL_H
#define DRMUTIL_H

#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#define PRINTFLIKE(f, a) __attribute__((format(printf, f, a)))
#define NORETURN __attribute__((noreturn))

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define ALIGN(v, a) (((v) + (a) - 1) & ~((a) - 1))

struct drm_init_params {
    int unused;
};

struct drm_plane {
    uint32_t id;
    uint32_t *formats;
    uint32_t format_count;
    uint32_t possible_crtcs;

    uint32_t fb_id;
    uint32_t crtc_id;
};

struct drm_crtc {
    uint32_t id;
    uint32_t gamma_size;

    struct drm_mode_modeinfo mode;
    uint32_t x;
    uint32_t y;
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

    struct drm_plane *planes;
    uint32_t plane_count;

    struct drm_crtc *crtcs;
    uint32_t crtc_count;

    struct drm_connector *connectors;
    uint32_t connector_count;
};

static inline void
drm_logv(const char *format, va_list ap)
{
    printf("DRM: ");
    vprintf(format, ap);
    printf("\n");
}

static inline void NORETURN
drm_diev(const char *format, va_list ap)
{
    drm_logv(format, ap);
    abort();
}

static inline void PRINTFLIKE(1, 2) drm_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    drm_logv(format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(1, 2) NORETURN drm_die(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    drm_diev(format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(2, 3) drm_check(const struct drm *drm, const char *format, ...)
{
    if (drm->ret >= 0)
        return;

    va_list ap;
    va_start(ap, format);
    drm_diev(format, ap);
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
    if (drm->connectors)
        free(drm->connectors);
    if (drm->crtcs)
        free(drm->crtcs);
    if (drm->planes)
        free(drm->planes);

    memset(drm->client_caps, 0, sizeof(drm->client_caps));
    memset(drm->caps, 0, sizeof(drm->caps));

    drmFreeVersion(drm->version);
    drm->version = NULL;

    drm->master = false;
    drm->node_type = DRM_NODE_MAX;

    close(drm->fd);
    drm->fd = -1;
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

    drm->plane_count = plane_res->count_planes;
    drm->planes = calloc(plane_res->count_planes, sizeof(*drm->planes));
    if (!drm->planes)
        drm_die("failed to alloc planes");
    for (uint32_t i = 0; i < plane_res->count_planes; i++) {
        const uint32_t res_id = plane_res->planes[i];
        struct drm_plane *dst = &drm->planes[i];
        drmModePlanePtr src = drmModeGetPlane(drm->fd, res_id);

        dst->id = src->plane_id;
        dst->formats = NULL;
        dst->format_count = src->count_formats;
        dst->possible_crtcs = src->possible_crtcs;

        dst->fb_id = src->fb_id;
        dst->crtc_id = src->crtc_id;

        if (src->crtc_x || src->crtc_y || src->x || src->y)
            drm_die("plane x/y is unexpectedly initialized by libdrm");
        if (src->gamma_size)
            drm_die("plane gamma is unexpectedly initialized by kernel");

        drmModeFreePlane(src);
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

        if (src->mode_valid) {
            static_assert(sizeof(dst->mode) == sizeof(src->mode), "");
            memcpy(&dst->mode, &src->mode, sizeof(src->mode));
        }
        dst->x = src->x;
        dst->y = src->y;

        drmModeFreeCrtc(src);
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
    }

    for (int i = 0; i < res->count_encoders; i++)
        drmModeFreeEncoder(encoders[i]);
    free(encoders);

    drmModeFreeResources(res);
    drmModeFreePlaneResources(plane_res);
}

#endif /* DRMUTIL_H */
