/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"
#include "wlutil.h"

struct wl_test {
    uint32_t width;
    uint32_t height;
    bool shm;

    struct wl wl;
    struct vk vk;

    struct wl_swapchain *swapchain;
    bool quit;
};

static void
wl_test_dispatch_key(void *data, uint32_t key)
{
    struct wl_test *test = data;

    switch (key) {
    case KEY_ESC:
    case KEY_Q:
        test->quit = true;
        break;
    default:
        break;
    }
}

static void
wl_test_dispatch_close(void *data)
{
    struct wl_test *test = data;

    test->quit = true;
}

static void
wl_test_dispatch_redraw(void *data)
{
    struct wl_test *test = data;
    struct wl *wl = &test->wl;

    if (test->shm) {
        const struct wl_swapchain_image *img = wl_acquire_swapchain_image(wl, test->swapchain);
        memset(img->data, 0x80,
               test->swapchain->width * test->swapchain->height *
                   wl_drm_format_cpp(test->swapchain->format));
        wl_present_swapchain_image(wl, test->swapchain, img);
    }
}

static void
wl_test_loop(struct wl_test *test)
{
    struct wl *wl = &test->wl;

    while (!test->quit)
        wl_dispatch(wl);
}

static void
wl_test_init(struct wl_test *test)
{
    struct wl *wl = &test->wl;
    struct vk *vk = &test->vk;

    const struct wl_init_params wl_params = {
        .data = test,
        .close = wl_test_dispatch_close,
        .redraw = wl_test_dispatch_redraw,
        .key = wl_test_dispatch_key,
    };
    wl_init(wl, &wl_params);
    vk_init(vk, NULL);

    test->swapchain = wl_create_swapchain(wl, test->width, test->height, DRM_FORMAT_XRGB8888, 3);

    if (test->shm)
        wl_init_swapchain_images_shm(wl, test->swapchain);
}

static void
wl_test_cleanup(struct wl_test *test)
{
    struct wl *wl = &test->wl;
    struct vk *vk = &test->vk;

    wl_destroy_swapchain(wl, test->swapchain);

    vk_cleanup(vk);
    wl_cleanup(wl);
}

int
main(void)
{
    struct wl_test test = {
        .width = 320,
        .height = 240,
        .shm = true,
    };

    wl_test_init(&test);
    wl_test_loop(&test);
    wl_test_cleanup(&test);

    return 0;
}
