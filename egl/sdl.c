/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "eglutil.h"
#include "sdlutil.h"

struct sdl_test {
    uint32_t width;
    uint32_t height;
    uint32_t win_flags;

    struct sdl sdl;

    PFNGLCLEARCOLORPROC ClearColor;
    PFNGLCLEARPROC Clear;
};

static void
sdl_test_init(struct sdl_test *test)
{
    struct sdl *sdl = &test->sdl;

    const struct sdl_init_params params = {
        .gl = true,
        .width = test->width,
        .height = test->height,
        .flags = test->win_flags,
    };
    sdl_init(sdl, &params);

    test->ClearColor = (PFNGLCLEARCOLORPROC)SDL_GL_GetProcAddress("glClearColor");
    test->Clear = (PFNGLCLEARPROC)SDL_GL_GetProcAddress("glClear");
}

static void
sdl_test_cleanup(struct sdl_test *test)
{
    struct sdl *sdl = &test->sdl;

    sdl_cleanup(sdl);
}

static void
sdl_test_draw(struct sdl_test *test)
{
    struct sdl *sdl = &test->sdl;

    while (true) {
        SDL_Event ev;
        if (!SDL_WaitEvent(&ev))
            egl_die("failed to wait event");
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
            redraw = ev.window.event == SDL_WINDOWEVENT_SHOWN ||
                     ev.window.event == SDL_WINDOWEVENT_EXPOSED;
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
            const uint32_t win_flags = SDL_GetWindowFlags(sdl->win);
            const uint32_t fs_flags =
                win_flags & SDL_WINDOW_FULLSCREEN ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP;
            SDL_SetWindowFullscreen(sdl->win, fs_flags);
        }

        if (toggle_minimize) {
            const uint32_t win_flags = SDL_GetWindowFlags(sdl->win);
            if (win_flags & SDL_WINDOW_MINIMIZED)
                SDL_RestoreWindow(sdl->win);
            else
                SDL_MinimizeWindow(sdl->win);
        }

        if (toggle_maximize) {
            const uint32_t win_flags = SDL_GetWindowFlags(sdl->win);
            if (win_flags & SDL_WINDOW_MAXIMIZED)
                SDL_RestoreWindow(sdl->win);
            else
                SDL_MaximizeWindow(sdl->win);
        }

        if (redraw) {
#if 0
            SDL_Surface *surf = SDL_GetWindowSurface(sdl->win);
            const uint32_t color = SDL_MapRGB(surf->format, 0xff, 0x80, 0x80);
            SDL_FillRect(surf, NULL, color);
            SDL_UpdateWindowSurface(sdl->win);
#else
            test->ClearColor(1.0f, 0.5f, 0.5f, 1.0f);
            test->Clear(GL_COLOR_BUFFER_BIT);
            SDL_GL_SwapWindow(sdl->win);
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
        .win_flags = SDL_WINDOW_OPENGL,
    };

    sdl_test_init(&test);
    sdl_test_draw(&test);
    sdl_test_cleanup(&test);

    return 0;
}
