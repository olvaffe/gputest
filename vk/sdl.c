/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "sdlutil.h"
#include "vkutil.h"

enum win_op {
    WIN_OP_NONE,
    WIN_OP_TOGGLE_MINIMIZED,
    WIN_OP_TOGGLE_MAXIMIZED,
    WIN_OP_TOGGLE_FULLSCREEN,
};

struct sdl_test {
    uint32_t win_width;
    uint32_t win_height;
    uint32_t win_flags;

    struct sdl sdl;
    struct vk vk;
    VkSurfaceKHR surf;

    bool quit;
    bool redraw;
    enum win_op win_op;

    struct vk_swapchain *swapchain;
};

static void
sdl_test_init(struct sdl_test *test)
{
    struct sdl *sdl = &test->sdl;
    struct vk *vk = &test->vk;

    const struct sdl_init_params sdl_params = {
        .vk = true,
        .libvulkan_path = LIBVULKAN_NAME,
        .width = test->win_width,
        .height = test->win_height,
        .flags = test->win_flags,
    };
    sdl_init(sdl, &sdl_params);

    const char *wsi_exts[8];
    uint32_t wsi_ext_count = ARRAY_SIZE(wsi_exts);
    if (!SDL_Vulkan_GetInstanceExtensions(sdl->win, &wsi_ext_count, wsi_exts))
        vk_die("failed to get wsi exts");

    const char *dev_exts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    const struct vk_init_params params = {
        .instance_exts = wsi_exts,
        .instance_ext_count = wsi_ext_count,
        .dev_exts = dev_exts,
        .dev_ext_count = ARRAY_SIZE(dev_exts),
    };
    vk_init(vk, &params);

    if (!SDL_Vulkan_CreateSurface(sdl->win, vk->instance, &test->surf))
        vk_die("failed to create surface");
}

static void
sdl_test_cleanup(struct sdl_test *test)
{
    struct sdl *sdl = &test->sdl;
    struct vk *vk = &test->vk;

    if (test->swapchain)
        vk_destroy_swapchain(vk, test->swapchain);

    vk->DestroySurfaceKHR(vk->instance, test->surf, NULL);
    vk_cleanup(vk);

    sdl_cleanup(sdl);
}

static void
sdl_test_draw(struct sdl_test *test, struct vk_image *img)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier barrier1 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = img->img,
        .subresourceRange = subres_range,
    };
    const VkImageMemoryBarrier barrier2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = img->img,
        .subresourceRange = subres_range,
    };

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0, 0, NULL, 0, NULL, 1, &barrier1);

    const VkClearColorValue clear_val = {
        .float32 = { 1.0f, 0.5f, 0.5f, 1.0f },
    };

    vk->CmdClearColorImage(cmd, img->img, barrier1.newLayout, &clear_val, 1, &subres_range);

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1,
                           &barrier2);

    vk_end_cmd(vk);
    vk_wait(vk);
}

static void
sdl_test_wait_events(struct sdl_test *test)
{
    struct sdl *sdl = &test->sdl;
    SDL_Event ev;
    int timeout = -1;
    while (SDL_WaitEventTimeout(&ev, timeout)) {
        timeout = 0;

        switch (ev.type) {
        case SDL_QUIT:
            test->quit = true;
            break;
        case SDL_WINDOWEVENT:
            sdl_log_event(&ev);
            switch (ev.window.event) {
            case SDL_WINDOWEVENT_SHOWN:
            case SDL_WINDOWEVENT_EXPOSED:
                test->redraw = true;
                break;
            default:
                break;
            }
            break;
        case SDL_KEYUP:
            switch (ev.key.keysym.sym) {
            case SDLK_f:
                test->win_op = WIN_OP_TOGGLE_FULLSCREEN;
                break;
            case SDLK_m:
                if (ev.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT))
                    test->win_op = WIN_OP_TOGGLE_MAXIMIZED;
                else
                    test->win_op = WIN_OP_TOGGLE_MINIMIZED;
                break;
            case SDLK_q:
            case SDLK_ESCAPE:
                test->quit = true;
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    }
    if (timeout == -1)
        vk_die("failed to wait for events");

    /* update win size */
    int win_width;
    int win_height;
    SDL_GetWindowSize(sdl->win, &win_width, &win_height);
    if (test->win_width != (unsigned)win_width || test->win_height != (unsigned)win_height) {
        vk_log("win resized: %dx%d -> %dx%d", test->win_width, test->win_height, win_width,
               win_height);
        test->win_width = win_width;
        test->win_height = win_height;
        test->redraw = true;
    }

    /* update win flags */
    test->win_flags = SDL_GetWindowFlags(sdl->win);

    if ((test->win_flags & SDL_WINDOW_HIDDEN) || !test->win_width || !test->win_height)
        test->redraw = false;
}

static void
sdl_test_redraw_window(struct sdl_test *test)
{
    struct vk *vk = &test->vk;

    if (!test->redraw)
        return;

    vk_log("redraw");
    test->redraw = false;

#if 0
    SDL_Surface *surf = SDL_GetWindowSurface(sdl->win);
    if (!surf)
        vk_die("no window surface");

    const uint32_t color = SDL_MapRGB(surf->format, 0xff, 0x80, 0x80);
    SDL_FillRect(surf, NULL, color);
    SDL_UpdateWindowSurface(sdl->win);
#else
    struct vk_image *img;

    if (!test->swapchain) {
        vk_log("create swapchain %dx%d", test->win_width, test->win_height);
        test->swapchain = vk_create_swapchain(
            vk, 0, test->surf, VK_FORMAT_B8G8R8A8_UNORM, test->win_width, test->win_height,
            VK_PRESENT_MODE_FIFO_KHR, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    }

    if (test->swapchain->info.imageExtent.width != test->win_width ||
        test->swapchain->info.imageExtent.height != test->win_height) {
        vk_log("re-create swapchain %dx%d -> %dx%d", test->swapchain->info.imageExtent.width,
               test->swapchain->info.imageExtent.height, test->win_width, test->win_height);
        vk_recreate_swapchain(vk, test->swapchain, test->win_width, test->win_height);
    }

    img = vk_acquire_swapchain_image(vk, test->swapchain);
    if (img) {
        sdl_test_draw(test, img);
        vk_present_swapchain_image(vk, test->swapchain);
    }
#endif
}

static void
sdl_test_configure_window(struct sdl_test *test)
{
    struct sdl *sdl = &test->sdl;

    switch (test->win_op) {
    case WIN_OP_TOGGLE_MINIMIZED:
        if (test->win_flags & SDL_WINDOW_MINIMIZED)
            SDL_RestoreWindow(sdl->win);
        else
            SDL_MinimizeWindow(sdl->win);
        break;
    case WIN_OP_TOGGLE_MAXIMIZED:
        if (test->win_flags & SDL_WINDOW_MAXIMIZED)
            SDL_RestoreWindow(sdl->win);
        else
            SDL_MaximizeWindow(sdl->win);
        break;
    case WIN_OP_TOGGLE_FULLSCREEN:
        SDL_SetWindowFullscreen(sdl->win, test->win_flags & SDL_WINDOW_FULLSCREEN
                                              ? 0
                                              : SDL_WINDOW_FULLSCREEN_DESKTOP);
        break;
    default:
        break;
    }

    test->win_op = WIN_OP_NONE;
}

static void
sdl_test_loop(struct sdl_test *test)
{
    while (true) {
        sdl_test_wait_events(test);

        if (test->quit)
            break;

        sdl_test_redraw_window(test);
        sdl_test_configure_window(test);
    }
}

int
main(void)
{
    struct sdl_test test = {
        .win_width = 320,
        .win_height = 240,
        .win_flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN,
    };

    sdl_test_init(&test);
    sdl_test_loop(&test);
    sdl_test_cleanup(&test);

    return 0;
}
