/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "drmutil.h"

static void
drm_dump_device(struct drm *drm, int idx)
{
    drmDevicePtr dev = drm->devices[idx];

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

    drm_open(drm, idx, DRM_NODE_PRIMARY);

    drm_log("  node type: %s", drm->node_type == DRM_NODE_PRIMARY ? "primary" : "render");
    drm_log("  master: %d", drm->master);
    drm_log("  kmd version: %d.%d.%d", drm->version->version_major, drm->version->version_minor,
            drm->version->version_patchlevel);
    drm_log("  kmd name: %s", drm->version->name);
    drm_log("  kmd date: %s", drm->version->date);
    drm_log("  kmd desc: %s", drm->version->desc);
    drm_log("  kmd caps:");
    drm_log("    dumb_buffer: %" PRIu64, drm->caps[DRM_CAP_DUMB_BUFFER]);
    drm_log("    vblank_high_crtc: %" PRIu64, drm->caps[DRM_CAP_VBLANK_HIGH_CRTC]);
    drm_log("    dumb_preferred_depth: %" PRIu64, drm->caps[DRM_CAP_DUMB_PREFERRED_DEPTH]);
    drm_log("    dumb_prefer_shadow: %" PRIu64, drm->caps[DRM_CAP_DUMB_PREFER_SHADOW]);
    drm_log("    prime: %" PRIu64, drm->caps[DRM_CAP_PRIME]);
    drm_log("    timestamp_monotonic: %" PRIu64, drm->caps[DRM_CAP_TIMESTAMP_MONOTONIC]);
    drm_log("    async_page_flip: %" PRIu64, drm->caps[DRM_CAP_ASYNC_PAGE_FLIP]);
    drm_log("    cursor_width: %" PRIu64, drm->caps[DRM_CAP_CURSOR_WIDTH]);
    drm_log("    cursor_height: %" PRIu64, drm->caps[DRM_CAP_CURSOR_HEIGHT]);
    drm_log("    addfb2_modifiers: %" PRIu64, drm->caps[DRM_CAP_ADDFB2_MODIFIERS]);
    drm_log("    page_flip_target: %" PRIu64, drm->caps[DRM_CAP_PAGE_FLIP_TARGET]);
    drm_log("    crtc_in_vblank_event: %" PRIu64, drm->caps[DRM_CAP_CRTC_IN_VBLANK_EVENT]);
    drm_log("    syncobj: %" PRIu64, drm->caps[DRM_CAP_SYNCOBJ]);
    drm_log("    syncobj_timeline: %" PRIu64, drm->caps[DRM_CAP_SYNCOBJ_TIMELINE]);
    drm_log("    atomic_async_page_flip: %" PRIu64, drm->caps[DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP]);
    drm_log("  kmd client caps:");
    drm_log("    stereo_3d: %" PRIu64, drm->client_caps[DRM_CLIENT_CAP_STEREO_3D]);
    drm_log("    universal_planes: %" PRIu64, drm->client_caps[DRM_CLIENT_CAP_UNIVERSAL_PLANES]);
    drm_log("    atomic: %" PRIu64, drm->client_caps[DRM_CLIENT_CAP_ATOMIC]);
    drm_log("    aspect_ratio: %" PRIu64, drm->client_caps[DRM_CLIENT_CAP_ASPECT_RATIO]);
    drm_log("    writeback_connectors: %" PRIu64,
            drm->client_caps[DRM_CLIENT_CAP_WRITEBACK_CONNECTORS]);
    drm_log("    cursor_plane_hotspot: %" PRIu64,
            drm->client_caps[DRM_CLIENT_CAP_CURSOR_PLANE_HOTSPOT]);

    drm_log("  fb count: %d", drm->resources->count_fbs);
    drm_log("  crtc count: %d", drm->resources->count_crtcs);
    drm_log("  connector count: %d", drm->resources->count_connectors);
    drm_log("  encoder count: %d", drm->resources->count_encoders);
    drm_log("  min size: (%d, %d)", drm->resources->min_width, drm->resources->min_height);
    drm_log("  max size: (%d, %d)", drm->resources->max_width, drm->resources->max_height);
    drm_log("  plane count: %d", drm->plane_resources->count_planes);

    drm_close(drm);
}

static void
drm_dump_devices(struct drm *drm)
{
    for (int i = 0; i < drm->device_count; i++)
        drm_dump_device(drm, i);
}

int
main(void)
{
    struct drm drm;
    drm_init(&drm, NULL);
    drm_dump_devices(&drm);
    drm_cleanup(&drm);

    return 0;
}
