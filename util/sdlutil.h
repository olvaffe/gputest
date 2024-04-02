/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "util.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

struct sdl_init_params {
    bool gl;
    bool vk;
    const char *libvulkan_path;

    int width;
    int height;
    uint32_t flags;
};

struct sdl {
    struct sdl_init_params params;

    SDL_Window *win;

    SDL_GLContext ctx;
};

static inline void PRINTFLIKE(1, 2) sdl_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_logv("SDL", format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(1, 2) NORETURN sdl_die(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_diev("SDL", format, ap);
    va_end(ap);
}

static inline void
sdl_init_video(struct sdl *sdl)
{
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "1");

    if (SDL_Init(SDL_INIT_VIDEO))
        sdl_die("failed to init sdl");

    if (sdl->params.gl) {
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    }

    if (sdl->params.vk && sdl->params.libvulkan_path) {
        if (SDL_Vulkan_LoadLibrary(sdl->params.libvulkan_path))
            sdl_die("failed to load vulkan into sdl");
    }
}

static inline void
sdl_init_window(struct sdl *sdl)
{
    sdl->win = SDL_CreateWindow("sdlutil", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                sdl->params.width, sdl->params.height, sdl->params.flags);
    if (!sdl->win)
        sdl_die("failed to create win");
}

static inline void
sdl_init_context(struct sdl *sdl)
{
    if (sdl->params.gl)
        sdl->ctx = SDL_GL_CreateContext(sdl->win);
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
    if (sdl->params.gl)
        SDL_GL_DeleteContext(sdl->ctx);

    SDL_DestroyWindow(sdl->win);

    if (sdl->params.vk)
        SDL_Vulkan_UnloadLibrary();

    SDL_Quit();
}

static inline void
sdl_log_event_windowevent(const SDL_Event *ev)
{
    switch (ev->window.event) {
#define CASE(ty)                                                                                 \
    case ty:                                                                                     \
        sdl_log("  " #ty);                                                                       \
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
        sdl_log("unknown windowe vent 0x%x", ev->window.event);
        break;
    }

    switch (ev->window.event) {
    case SDL_WINDOWEVENT_RESIZED:
    case SDL_WINDOWEVENT_SIZE_CHANGED:
        sdl_log("  data1 %d data2 %d", ev->window.data1, ev->window.data2);
        break;
    default:
        break;
    }
}

static inline void
sdl_log_event(const SDL_Event *ev)
{
    switch (ev->type) {
#define CASE(ty)                                                                                 \
    case ty:                                                                                     \
        sdl_log(#ty);                                                                            \
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
        sdl_log("unknown event 0x%x", ev->type);
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
