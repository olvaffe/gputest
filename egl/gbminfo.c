/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "gbmutil.h"

static void
gbm_test_bo(struct gbm *gbm)
{
    const uint32_t flags = 0;

    struct gbm_import_fd_modifier_data data = {
        .width = 64,
        .height = 64,
        .format = GBM_FORMAT_ARGB8888,
        .modifier = DRM_FORMAT_MOD_LINEAR,
    };
    struct gbm_bo *bo =
        gbm_create_bo(gbm, data.width, data.height, data.format, &data.modifier, 1, flags);

    /* test map/unmap */
    uint32_t stride;
    void *ptr = gbm_map_bo(gbm, bo, GBM_BO_TRANSFER_WRITE, &stride);
    memset(ptr, 0x7f, data.height * stride);
    gbm_unmap_bo(gbm, bo);

    /* test export */
    gbm_export_bo(gbm, bo, &data);

    /* test import */
    struct gbm_bo *bo2 = gbm_create_bo_from_dmabuf(gbm, &data, flags);
    gbm_destroy_bo(gbm, bo2);

    gbm_destroy_bo(gbm, bo);

    /* test import again */
    bo = gbm_create_bo_from_dmabuf(gbm, &data, flags);
    gbm_destroy_bo(gbm, bo);

    for (uint32_t i = 0; i < data.num_fds; i++)
        close(data.fds[i]);
}

static void
gbm_dump(struct gbm *gbm)
{
    gbm_log("backend: %s", gbm->backend_name);
    for (uint32_t i = 0; i < gbm->format_count; i++) {
        const struct gbm_format_info *info = &gbm->formats[i];
        gbm_log("format: %.*s", 4, (const char *)&info->format);
        for (uint32_t j = 0; j < info->modifier_count; j++)
            gbm_log("  mod: %" PRIx64, info->modifiers[j]);
    }
}

int
main(int argc, char **argv)
{
    if (argc != 2)
        gbm_die("usage: %s <device-path>", argv[0]);
    const char *path = argv[1];

    struct gbm gbm;
    const struct gbm_init_params params = {
        .path = path,
    };
    gbm_init(&gbm, &params);
    gbm_dump(&gbm);
    gbm_test_bo(&gbm);
    gbm_cleanup(&gbm);

    return 0;
}
