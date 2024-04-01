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
#define ALIGN(v, a) (((v) + (a)-1) & ~((a)-1))

struct drm_init_params {
    int unused;
};

struct drm {
    struct drm_init_params params;
    int ret;

    drmDevicePtr *devices;
    int device_count;

    int fd;
    int node_type;
    bool master;
    drmVersionPtr version;
    uint64_t caps[64];
    uint64_t client_caps[64];
    drmModeResPtr resources;
    drmModePlaneResPtr plane_resources;
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

    const int count = drm->ret;
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
drm_open(struct drm *drm, int idx, int type)
{
    drmDevicePtr dev = drm->devices[idx];
    if (!(dev->available_nodes & (1 << type)))
        drm_die("bad node type");

    drm->fd = open(dev->nodes[type], O_RDWR);
    if (drm->fd < 0)
        drm_die("failed to open %s", dev->nodes[type]);

    drm->node_type = type;
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

    if (type == DRM_NODE_PRIMARY) {
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

        drm->resources = drmModeGetResources(drm->fd);
        drm->plane_resources = drmModeGetPlaneResources(drm->fd);
    }
}

static inline void
drm_close(struct drm *drm)
{
    if (drm->node_type == DRM_NODE_PRIMARY) {
        drmModeFreePlaneResources(drm->plane_resources);
        drmModeFreeResources(drm->resources);

        memset(drm->client_caps, 0, sizeof(drm->client_caps));
    }

    memset(drm->caps, 0, sizeof(drm->caps));

    drmFreeVersion(drm->version);
    drm->version = NULL;

    close(drm->fd);
    drm->fd = -1;
}

#endif /* DRMUTIL_H */
