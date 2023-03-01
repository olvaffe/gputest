/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

struct sdl_test {
    uint32_t width;
    uint32_t height;
    uint32_t win_flags;

    SDL_Window *win;

    struct vk vk;
    VkSurfaceKHR surf;
    struct vk_swapchain *swapchain;
};

static void
sdl_log_event_windowevent(const SDL_Event *ev)
{
    switch (ev->window.event) {
#define CASE(ty)                                                                                 \
    case ty:                                                                                     \
        vk_log("  " #ty);                                                                        \
        break
        CASE(SDL_WINDOWEVENT_SHOWN);
        CASE(SDL_WINDOWEVENT_HIDDEN);
        CASE(SDL_WINDOWEVENT_EXPOSED);
        CASE(SDL_WINDOWEVENT_MOVED);
        CASE(SDL_WINDOWEVENT_RESIZED);
        CASE(SDL_WINDOWEVENT_SIZE_CHANGED);
        CASE(SDL_WINDOWEVENT_MINIMIZED);
        CASE(SDL_WINDOWEVENT_MAXIMIZED);
        CASE(SDL_WINDOWEVENT_RESTORED);
        CASE(SDL_WINDOWEVENT_ENTER);
        CASE(SDL_WINDOWEVENT_LEAVE);
        CASE(SDL_WINDOWEVENT_FOCUS_GAINED);
        CASE(SDL_WINDOWEVENT_FOCUS_LOST);
        CASE(SDL_WINDOWEVENT_CLOSE);
        CASE(SDL_WINDOWEVENT_TAKE_FOCUS);
        CASE(SDL_WINDOWEVENT_HIT_TEST);
        CASE(SDL_WINDOWEVENT_ICCPROF_CHANGED);
        CASE(SDL_WINDOWEVENT_DISPLAY_CHANGED);
#undef CASE
    default:
        vk_log("unknown windowe vent 0x%x", ev->window.event);
        break;
    }
}

static void
sdl_log_event(const SDL_Event *ev)
{
    switch (ev->type) {
#define CASE(ty)                                                                                 \
    case ty:                                                                                     \
        vk_log(#ty);                                                                             \
        break
        CASE(SDL_QUIT);
        CASE(SDL_APP_TERMINATING);
        CASE(SDL_APP_LOWMEMORY);
        CASE(SDL_APP_WILLENTERBACKGROUND);
        CASE(SDL_APP_DIDENTERBACKGROUND);
        CASE(SDL_APP_WILLENTERFOREGROUND);
        CASE(SDL_APP_DIDENTERFOREGROUND);
        CASE(SDL_LOCALECHANGED);
        CASE(SDL_DISPLAYEVENT);
        CASE(SDL_WINDOWEVENT);
        CASE(SDL_SYSWMEVENT);
        CASE(SDL_KEYDOWN);
        CASE(SDL_KEYUP);
        CASE(SDL_TEXTEDITING);
        CASE(SDL_TEXTINPUT);
        CASE(SDL_KEYMAPCHANGED);
        CASE(SDL_TEXTEDITING_EXT);
        CASE(SDL_MOUSEMOTION);
        CASE(SDL_MOUSEBUTTONDOWN);
        CASE(SDL_MOUSEBUTTONUP);
        CASE(SDL_MOUSEWHEEL);
        CASE(SDL_JOYAXISMOTION);
        CASE(SDL_JOYBALLMOTION);
        CASE(SDL_JOYHATMOTION);
        CASE(SDL_JOYBUTTONDOWN);
        CASE(SDL_JOYBUTTONUP);
        CASE(SDL_JOYDEVICEADDED);
        CASE(SDL_JOYDEVICEREMOVED);
        CASE(SDL_JOYBATTERYUPDATED);
        CASE(SDL_CONTROLLERAXISMOTION);
        CASE(SDL_CONTROLLERBUTTONDOWN);
        CASE(SDL_CONTROLLERBUTTONUP);
        CASE(SDL_CONTROLLERDEVICEADDED);
        CASE(SDL_CONTROLLERDEVICEREMOVED);
        CASE(SDL_CONTROLLERDEVICEREMAPPED);
        CASE(SDL_CONTROLLERTOUCHPADDOWN);
        CASE(SDL_CONTROLLERTOUCHPADMOTION);
        CASE(SDL_CONTROLLERTOUCHPADUP);
        CASE(SDL_CONTROLLERSENSORUPDATE);
        CASE(SDL_FINGERDOWN);
        CASE(SDL_FINGERUP);
        CASE(SDL_FINGERMOTION);
        CASE(SDL_DOLLARGESTURE);
        CASE(SDL_DOLLARRECORD);
        CASE(SDL_MULTIGESTURE);
        CASE(SDL_CLIPBOARDUPDATE);
        CASE(SDL_DROPFILE);
        CASE(SDL_DROPTEXT);
        CASE(SDL_DROPBEGIN);
        CASE(SDL_DROPCOMPLETE);
        CASE(SDL_AUDIODEVICEADDED);
        CASE(SDL_AUDIODEVICEREMOVED);
        CASE(SDL_SENSORUPDATE);
        CASE(SDL_RENDER_TARGETS_RESET);
        CASE(SDL_RENDER_DEVICE_RESET);
        CASE(SDL_POLLSENTINEL);
        CASE(SDL_USEREVENT);
#undef CASE
    default:
        vk_log("unknown event 0x%x", ev->type);
        break;
    }

    switch (ev->type) {
    case SDL_WINDOWEVENT:
        sdl_log_event_windowevent(ev);
        break;
    default:
        break;
    }
}

static void
sdl_test_init(struct sdl_test *test)
{
    struct vk *vk = &test->vk;

    if (SDL_Init(SDL_INIT_VIDEO))
        vk_die("failed to init sdl");

    if (SDL_Vulkan_LoadLibrary(LIBVULKAN_NAME))
        vk_die("failed to load vulkan into sdl");

    test->win = SDL_CreateWindow("test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                 test->width, test->height, test->win_flags);
    if (!test->win)
        vk_die("failed to create win");

    const char *wsi_exts[8];
    uint32_t wsi_ext_count = ARRAY_SIZE(wsi_exts);
    if (!SDL_Vulkan_GetInstanceExtensions(test->win, &wsi_ext_count, wsi_exts))
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

    if (!SDL_Vulkan_CreateSurface(test->win, vk->instance, &test->surf))
        vk_die("failed to create surface");

    test->swapchain =
        vk_create_swapchain(vk, test->surf, VK_FORMAT_B8G8R8A8_UNORM, test->width, test->height,
                            VK_PRESENT_MODE_FIFO_KHR, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
}

static void
sdl_test_cleanup(struct sdl_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_swapchain(vk, test->swapchain);
    vk->DestroySurfaceKHR(vk->instance, test->surf, NULL);
    vk_cleanup(vk);

    SDL_DestroyWindow(test->win);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();
}

static void
sdl_test_draw(struct sdl_test *test, struct vk_image *img)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk);

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
sdl_test_loop(struct sdl_test *test)
{
    struct vk *vk = &test->vk;

    while (true) {
        SDL_Event ev;
        if (!SDL_WaitEvent(&ev))
            vk_die("failed to wait event");
        sdl_log_event(&ev);

        bool quit = false;
        bool redraw = false;
        bool toggle_fullscreen = false;
        bool toggle_minimize = false;
        bool toggle_maximize = false;

        switch (ev.type) {
        case SDL_QUIT:
            quit = true;
            break;
        case SDL_WINDOWEVENT:
            redraw = ev.window.event == SDL_WINDOWEVENT_EXPOSED;
            break;
        case SDL_KEYUP:
            switch (ev.key.keysym.sym) {
            case SDLK_f:
                toggle_fullscreen = true;
                break;
            case SDLK_m:
                if (ev.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT))
                    toggle_maximize = true;
                else
                    toggle_minimize = true;
                break;
            case SDLK_q:
                quit = true;
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }

        if (quit)
            break;

        if (toggle_fullscreen) {
            const uint32_t win_flags = SDL_GetWindowFlags(test->win);
            const uint32_t fs_flags =
                win_flags & SDL_WINDOW_FULLSCREEN ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP;
            SDL_SetWindowFullscreen(test->win, fs_flags);
        }

        if (toggle_minimize) {
            const uint32_t win_flags = SDL_GetWindowFlags(test->win);
            if (win_flags & SDL_WINDOW_MINIMIZED)
                SDL_RestoreWindow(test->win);
            else
                SDL_MinimizeWindow(test->win);
        }

        if (toggle_maximize) {
            const uint32_t win_flags = SDL_GetWindowFlags(test->win);
            if (win_flags & SDL_WINDOW_MAXIMIZED)
                SDL_RestoreWindow(test->win);
            else
                SDL_MaximizeWindow(test->win);
        }

        if (redraw) {
#if 0
            SDL_Surface *surf = SDL_GetWindowSurface(test->win);
            assert(surf);
            const uint32_t color = SDL_MapRGB(surf->format, 0xff, 0x80, 0x80);
            SDL_FillRect(surf, NULL, color);
            SDL_UpdateWindowSurface(test->win);
#else
            struct vk_image *img = vk_acquire_swapchain_image(vk, test->swapchain);
            sdl_test_draw(test, img);
            vk_present_swapchain_image(vk, test->swapchain);
#endif
        }
    }
}

int
main(void)
{
    struct sdl_test test = {
        .width = 320,
        .height = 240,
        .win_flags = SDL_WINDOW_VULKAN,
    };

    sdl_test_init(&test);
    sdl_test_loop(&test);
    sdl_test_cleanup(&test);

    return 0;
}
