/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "drmutil.h"

static bool opt_verbose = false;

static void
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

static void
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
    case DRM_BUS_PLATFORM:
        drm_log("  bus type: platform");
        drm_log("  bus info: %s", dev->businfo.platform->fullname);
        drm_log("  dev info:");
        for (int i = 0; dev->deviceinfo.platform->compatible[i]; i++)
            drm_log("    %s", dev->deviceinfo.platform->compatible[i]);
        break;
    default:
        drm_log("  bus type %d", dev->bustype);
        break;
    }
}

static void
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

static void
drm_dump_properties(struct drm *drm, const struct drm_properties *props, const char *indent)
{
    for (uint32_t i = 0; i < props->count; i++)
        drm_dump_property(drm, props->props[i], props->values[i], indent);
}

static void
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

static void
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

static void
drm_dump_devices(struct drm *drm)
{
    for (uint32_t i = 0; i < drm->device_count; i++) {
        drm_dump_device(drm, i);

        drm_open(drm, i, DRM_NODE_PRIMARY);
        drm_log("device %d opened", i);
        drm_dump_file(drm);

        if (drm->file.node_type == DRM_NODE_PRIMARY) {
            drm_scan_resources(drm);
            drm_log("device %d scanned", i);

            drm_dump_modeset(drm, opt_verbose);

            drm_release_resources(drm);
        }

        drm_close(drm);
    }
}

int
main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v"))
            opt_verbose = true;
    }

    struct drm drm;
    drm_init(&drm, NULL);
    drm_dump_devices(&drm);
    drm_cleanup(&drm);

    return 0;
}
