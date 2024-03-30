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
    int node_type = -1;
    for (int i = 0; i < DRM_NODE_MAX; i++) {
        if (!(dev->available_nodes & (1 << i)))
            continue;
        drm_log("  node type %d: %s", i, dev->nodes[i]);
        node_type = i;
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

    drm_open(drm, idx, node_type);
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
