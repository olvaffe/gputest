/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "drmutil.h"

static void
drm_dump_primary_device(struct drm *drm, uint32_t idx)
{
    drm_log("device %d client caps set", idx);
    drm_log("  stereo_3d: %" PRIu64, drm->client_caps[DRM_CLIENT_CAP_STEREO_3D]);
    drm_log("  universal_planes: %" PRIu64, drm->client_caps[DRM_CLIENT_CAP_UNIVERSAL_PLANES]);
    drm_log("  atomic: %" PRIu64, drm->client_caps[DRM_CLIENT_CAP_ATOMIC]);
    drm_log("  aspect_ratio: %" PRIu64, drm->client_caps[DRM_CLIENT_CAP_ASPECT_RATIO]);
    drm_log("  writeback_connectors: %" PRIu64,
            drm->client_caps[DRM_CLIENT_CAP_WRITEBACK_CONNECTORS]);
    drm_log("  cursor_plane_hotspot: %" PRIu64,
            drm->client_caps[DRM_CLIENT_CAP_CURSOR_PLANE_HOTSPOT]);

    drm_scan_resources(drm);
    drm_log("device %d scanned", idx);

    drm_log("  min size: %dx%d", drm->min_width, drm->min_height);
    drm_log("  max size: %dx%d", drm->max_width, drm->max_height);

    drm_log("  plane count: %d", drm->plane_count);
    for (uint32_t i = 0; i < drm->plane_count; i++) {
        const struct drm_plane *plane = &drm->planes[i];
        drm_log("    plane[%d]: id %d, fb %d, crtc %d, mask 0x%x", i, plane->id, plane->fb_id,
                plane->crtc_id, plane->possible_crtcs);
        // drm_log("    plane[%d] format count: %d", i, plane->format_count);
    }

    drm_log("  crtc count: %d", drm->crtc_count);
    for (uint32_t i = 0; i < drm->crtc_count; i++) {
        const struct drm_crtc *crtc = &drm->crtcs[i];
        drm_log("    crtc[%d]: id %d, mode %s, offset %dx%d, gamma %d", i, crtc->id,
                crtc->mode.name[0] != '\0' ? crtc->mode.name : "invalid", crtc->x, crtc->y,
                crtc->gamma_size);
    }

    drm_log("  connector count: %d", drm->connector_count);
    for (uint32_t i = 0; i < drm->connector_count; i++) {
        const struct drm_connector *connector = &drm->connectors[i];
        drm_log("    connector[%d]: id %d, crtc %d, connected %d, type %s-%d, size %dx%d, "
                "mask 0x%x",
                i, connector->id, connector->crtc_id, connector->connected,
                drmModeGetConnectorTypeName(connector->type), connector->type_id,
                connector->width_mm, connector->height_mm, connector->possible_crtcs);
    }
}

static void
drm_dump_device(struct drm *drm, uint32_t idx)
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
    drm_log("device %d opened", idx);

    drm_log("  fd node type: %s", drm->node_type == DRM_NODE_PRIMARY ? "primary" : "render");
    drm_log("  fd master: %d", drm->master);
    drm_log("  version: %d.%d.%d", drm->version->version_major, drm->version->version_minor,
            drm->version->version_patchlevel);
    drm_log("    name: %s", drm->version->name);
    drm_log("    date: %s", drm->version->date);
    drm_log("    desc: %s", drm->version->desc);
    drm_log("  caps:");
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

    if (drm->node_type == DRM_NODE_PRIMARY)
        drm_dump_primary_device(drm, idx);

    drm_close(drm);
}

static void
drm_dump_devices(struct drm *drm)
{
    for (uint32_t i = 0; i < drm->device_count; i++)
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
