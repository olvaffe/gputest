/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "drmutil.h"
#include "ffutil.h"
#include "vautil.h"

struct ffdec_test {
    const char *filename;

    struct drm drm;
    struct va va;
    struct ff ff;
};

static void
ffdec_test_init(struct ffdec_test *test)
{
    struct drm *drm = &test->drm;
    struct va *va = &test->va;
    struct ff *ff = &test->ff;

    drm_init(drm, NULL);
    drm_open(drm, 0, DRM_NODE_RENDER);

    const struct va_init_params params = {
        .drm_fd = drm->fd,
    };
    va_init(va, &params);

    ff_init(ff, va->display, test->filename);
}

static void
ffdec_test_cleanup(struct ffdec_test *test)
{
    struct drm *drm = &test->drm;
    struct va *va = &test->va;
    struct ff *ff = &test->ff;

    ff_cleanup(ff);
    va_cleanup(va);

    drm_close(drm);
    drm_cleanup(drm);
}

static void
ffdec_test_decode(struct ffdec_test *test)
{
    struct va *va = &test->va;
    struct ff *ff = &test->ff;

    const uint64_t start_time = u_now();
    int frame_idx = 0;
    while (ff_decode_frame(ff)) {
        const uint32_t flags = VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_COMPOSED_LAYERS;
        VADRMPRIMESurfaceDescriptor desc;
        va_export_surface(va, ff_get_frame_surface(ff), flags, &desc);

        if (desc.num_objects != 1)
            va_die("unexpected disjoint surface");
        if (desc.num_layers != 1)
            va_die("unexpected separate-layer surface");
        if (desc.fourcc != desc.layers[0].drm_format)
            va_die("bad surface fourcc");

        if (!frame_idx) {
            va_log("fourcc %.*s, size %ux%u, bo size %u, modifier 0x%" PRIx64, 4,
                   (const char *)&desc.fourcc, desc.width, desc.height, desc.objects[0].size,
                   desc.objects[0].drm_format_modifier);
            for (uint32_t i = 0; i < desc.layers[0].num_planes; i++) {
                if (desc.layers[0].object_index[i] != 0)
                    va_die("bad surface object index");
                va_log("  plane %d: offset %u, pitch %u", i, desc.layers[0].offset[i],
                       desc.layers[0].pitch[i]);
            }
        }

        close(desc.objects[0].fd);

        frame_idx++;
    }
    const uint64_t decode_time = u_now() - start_time;
    const uint32_t decode_ms = decode_time / 1000000;

    va_log("decoded %d frames in %d.%ds", frame_idx, decode_ms / 1000, decode_ms % 1000);
}

int
main(int argc, char **argv)
{
    struct ffdec_test test = {};

    if (argc != 2)
        va_die("usage: %s <file>", argv[0]);
    test.filename = argv[1];

    ffdec_test_init(&test);
    ffdec_test_decode(&test);
    ffdec_test_cleanup(&test);

    return 0;
}
