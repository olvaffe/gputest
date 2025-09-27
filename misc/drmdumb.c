/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "dmautil.h"
#include "drmutil.h"

struct drmdumb_test {
    int dev_index;
    uint32_t format;

    struct drm drm;

    const struct drm_crtc *crtc;
    const struct drm_plane *plane;
    const struct drm_connector *connector;
    const struct drm_mode_modeinfo *mode;

    struct drm_dumb *dumb;
};

static void
drmdumb_test_init_req(struct drmdumb_test *test)
{
    struct drm *drm = &test->drm;

    drm_reset_req(drm);
    drm_add_property(drm, test->plane->id, test->plane->properties, "FB_ID", test->dumb->fb_id);
}

static void
drmdumb_test_init_dumb(struct drmdumb_test *test)
{
    struct drm *drm = &test->drm;

    test->dumb = drm_create_dumb(drm, test->mode->hdisplay, test->mode->vdisplay, test->format);

    drm_map_dumb(drm, test->dumb);

    if (test->format == DRM_FORMAT_XRGB8888) {
        for (uint32_t y = 0; y < test->mode->vdisplay; y++) {
            for (uint32_t x = 0; x < test->mode->hdisplay; x++) {
                const uint8_t r = (x / 4) % 256;
                const uint8_t g = (y / 4) % 256;
                const uint8_t b = x == y ? 255 : 0;
                const uint32_t xrgb = (r % 256) << 16 | (g % 256) << 8 | (b % 256) << 0;

                uint32_t *row = test->dumb->map + test->dumb->pitch * y;
                row[x] = xrgb;
            }
        }
    } else {
        memset(test->dumb->map, 0x80, test->dumb->size);
    }

    drm_unmap_dumb(drm, test->dumb);
}

static void
drmdumb_test_init_pipe(struct drmdumb_test *test)
{
    struct drm *drm = &test->drm;

    for (uint32_t i = 0; i < drm->modeset.connector_count; i++) {
        const struct drm_connector *connector = &drm->modeset.connectors[i];

        /* use the first active connector */
        if (connector->crtc_id && connector->connected) {
            test->connector = connector;
            break;
        }
    }
    if (!test->connector)
        drm_die("no active connector");

    for (uint32_t i = 0; i < drm->modeset.crtc_count; i++) {
        const struct drm_crtc *crtc = &drm->modeset.crtcs[i];

        /* use the active crtc */
        if (crtc->id == test->connector->crtc_id) {
            test->crtc = crtc;
            break;
        }
    }
    if (!test->crtc)
        drm_die("no active crtc");

    /* use the active mode */
    if (test->crtc->mode_valid)
        test->mode = &test->crtc->mode;
    else
        drm_die("no valid mode");

    for (uint32_t i = 0; i < drm->modeset.plane_count; i++) {
        const struct drm_plane *plane = &drm->modeset.planes[i];

        /* use the active plane */
        if (plane->crtc_id == test->crtc->id) {
            test->plane = plane;
            break;
        }
    }
    if (!test->plane)
        drm_die("no active plane");

    bool has_format = false;
    for (uint32_t i = 0; i < test->plane->format_count; i++) {
        if (test->plane->formats[i] == test->format) {
            has_format = true;
            break;
        }
    }
    if (!has_format)
        drm_die("no format");
}

static void
drmdumb_test_init(struct drmdumb_test *test)
{
    struct drm *drm = &test->drm;

    drm_init(drm, NULL);
    drm_open(drm, test->dev_index, DRM_NODE_PRIMARY);
    drm_scan_resources(drm);

    drmdumb_test_init_pipe(test);
    drmdumb_test_init_dumb(test);
    drmdumb_test_init_req(test);
}

static void
drmdumb_test_cleanup(struct drmdumb_test *test)
{
    struct drm *drm = &test->drm;

    drm_destroy_dumb(drm, test->dumb);

    drm_release_resources(drm);
    drm_close(drm);
    drm_cleanup(drm);
}

static void
drmdumb_test_prime(struct drmdumb_test *test)
{
    struct drm *drm = &test->drm;

    const int fd = drm_prime_export(drm, test->dumb->handle);

    /* test import */
    {
        const int fd2 = dup(fd);
        if (fd2 < 0)
            drm_die("failed to dup");

        const uint32_t handle = drm_prime_import(drm, fd2);
        if (test->dumb->handle != handle)
            drm_die("re-import returned bad handle");
    }

    /* test access through dma-buf */
    struct dma_buf *buf = dma_buf_create(fd);
    dma_buf_map(buf);
    dma_buf_start(buf, DMA_BUF_SYNC_WRITE);
    memset(buf->map, 0xff, test->dumb->pitch * 10);
    dma_buf_end(buf);
    dma_buf_unmap(buf);
    dma_buf_destroy(buf);
}

static void
drmdumb_test_commit(struct drmdumb_test *test)
{
    struct drm *drm = &test->drm;

    drm_commit(drm);
    u_sleep(1000);
}

int
main(int argc, char **argv)
{
    struct drmdumb_test test = {
        .dev_index = 0,
        .format = DRM_FORMAT_XRGB8888,
    };

    drmdumb_test_init(&test);
    drmdumb_test_prime(&test);
    drmdumb_test_commit(&test);
    drmdumb_test_cleanup(&test);

    return 0;
}
