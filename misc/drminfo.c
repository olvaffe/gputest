/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "drmutil.h"

static bool opt_verbose = false;

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
