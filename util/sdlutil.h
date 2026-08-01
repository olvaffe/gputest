/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef SDLUTIL_H
#define SDLUTIL_H

#include "util.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#define sdl_die(format, ...) u_die("SDL", format __VA_OPT__(, ) __VA_ARGS__)
#define sdl_log(format, ...) u_log("SDL", format __VA_OPT__(, ) __VA_ARGS__)

struct sdl_init_params {
    const char *libvulkan_path;

    int width;
    int height;
    SDL_WindowFlags flags;
};

struct sdl {
    struct sdl_init_params params;

    SDL_Window *win;

    SDL_GLContext ctx;

    uint32_t wsi_ext_count;
    const char *const *wsi_exts;
};

static inline void
sdl_init_video(struct sdl *sdl)
{
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "1");

    if (!SDL_Init(SDL_INIT_VIDEO))
        sdl_die("failed to init sdl video: %s", SDL_GetError());

    if (sdl->params.flags & SDL_WINDOW_OPENGL) {
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    }

    if (sdl->params.flags & SDL_WINDOW_VULKAN) {
        if (sdl->params.libvulkan_path) {
            if (!SDL_Vulkan_LoadLibrary(sdl->params.libvulkan_path))
                sdl_die("failed to load vulkan into sdl: %s", SDL_GetError());
        }

        sdl->wsi_exts = SDL_Vulkan_GetInstanceExtensions(&sdl->wsi_ext_count);
        if (!sdl->wsi_exts)
            sdl_die("failed to get vulkan wsi extensions: %s", SDL_GetError());
    }
}

static inline void
sdl_init_window(struct sdl *sdl)
{
    sdl->win =
        SDL_CreateWindow("sdlutil", sdl->params.width, sdl->params.height, sdl->params.flags);
    if (!sdl->win)
        sdl_die("failed to create win: %s", SDL_GetError());
}

static inline void
sdl_init_context(struct sdl *sdl)
{
    if (sdl->params.flags & SDL_WINDOW_OPENGL) {
        sdl->ctx = SDL_GL_CreateContext(sdl->win);
        if (!sdl->ctx)
            sdl_die("failed to create gl context: %s", SDL_GetError());
    }
}

static inline void
sdl_init(struct sdl *sdl, const struct sdl_init_params *params)
{
    sdl->params = *params;

    sdl_init_video(sdl);
    sdl_init_window(sdl);
    sdl_init_context(sdl);
}

static inline void
sdl_cleanup(struct sdl *sdl)
{
    if (sdl->params.flags & SDL_WINDOW_OPENGL)
        SDL_GL_DestroyContext(sdl->ctx);

    SDL_DestroyWindow(sdl->win);

    if (sdl->params.flags & SDL_WINDOW_VULKAN)
        SDL_Vulkan_UnloadLibrary();

    SDL_Quit();
}

static inline void
sdl_log_event(const SDL_Event *ev)
{
    switch (ev->type) {
#define CASE(ty)                                                                                 \
    case ty:                                                                                     \
        sdl_log(#ty);                                                                            \
        break
        /* application events */
        CASE(SDL_EVENT_QUIT);
        CASE(SDL_EVENT_TERMINATING);
        /* window events */
        CASE(SDL_EVENT_WINDOW_SHOWN);
        CASE(SDL_EVENT_WINDOW_HIDDEN);
        CASE(SDL_EVENT_WINDOW_EXPOSED);
        CASE(SDL_EVENT_WINDOW_MOVED);
        CASE(SDL_EVENT_WINDOW_RESIZED);
        CASE(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED);
        CASE(SDL_EVENT_WINDOW_METAL_VIEW_RESIZED);
        CASE(SDL_EVENT_WINDOW_MINIMIZED);
        CASE(SDL_EVENT_WINDOW_MAXIMIZED);
        CASE(SDL_EVENT_WINDOW_RESTORED);
        CASE(SDL_EVENT_WINDOW_MOUSE_ENTER);
        CASE(SDL_EVENT_WINDOW_MOUSE_LEAVE);
        CASE(SDL_EVENT_WINDOW_FOCUS_GAINED);
        CASE(SDL_EVENT_WINDOW_FOCUS_LOST);
        CASE(SDL_EVENT_WINDOW_CLOSE_REQUESTED);
        CASE(SDL_EVENT_WINDOW_HIT_TEST);
        CASE(SDL_EVENT_WINDOW_ICCPROF_CHANGED);
        CASE(SDL_EVENT_WINDOW_DISPLAY_CHANGED);
        CASE(SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED);
        CASE(SDL_EVENT_WINDOW_SAFE_AREA_CHANGED);
        CASE(SDL_EVENT_WINDOW_OCCLUDED);
        CASE(SDL_EVENT_WINDOW_ENTER_FULLSCREEN);
        CASE(SDL_EVENT_WINDOW_LEAVE_FULLSCREEN);
        CASE(SDL_EVENT_WINDOW_DESTROYED);
        CASE(SDL_EVENT_WINDOW_HDR_STATE_CHANGED);
        /* keyboard events */
        CASE(SDL_EVENT_KEY_DOWN);
        CASE(SDL_EVENT_KEY_UP);
        CASE(SDL_EVENT_KEYMAP_CHANGED);
        CASE(SDL_EVENT_KEYBOARD_ADDED);
        CASE(SDL_EVENT_KEYBOARD_REMOVED);
        /* mouse events */
        CASE(SDL_EVENT_MOUSE_MOTION);
        CASE(SDL_EVENT_MOUSE_BUTTON_DOWN);
        CASE(SDL_EVENT_MOUSE_BUTTON_UP);
        CASE(SDL_EVENT_MOUSE_WHEEL);
        CASE(SDL_EVENT_MOUSE_ADDED);
        CASE(SDL_EVENT_MOUSE_REMOVED);
        /* clipboard events */
        CASE(SDL_EVENT_CLIPBOARD_UPDATE);
#undef CASE
    default:
        sdl_log("unknown event 0x%" PRIx32, ev->type);
        break;
    }

    switch (ev->type) {
    case SDL_EVENT_WINDOW_MOVED:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        sdl_log("  data1 %" PRId32 " data2 %" PRId32, ev->window.data1, ev->window.data2);
        break;
    default:
        break;
    }
}

#endif /* SDLUTIL_H */
