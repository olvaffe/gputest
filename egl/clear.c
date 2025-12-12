/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

/* This test clears an fbo to red and dumps it to a file. */

#include "eglutil.h"

struct clear_test {
    uint32_t width;
    uint32_t height;
    float color[4];

    struct egl egl;
    struct egl_framebuffer *fb;
};

static void
clear_test_init(struct clear_test *test)
{
    struct egl *egl = &test->egl;

    egl_init(egl, NULL);
    test->fb = egl_create_framebuffer(egl, test->width, test->height, GL_RGBA8, GL_NONE);
}

static void
clear_test_cleanup(struct clear_test *test)
{
    struct egl *egl = &test->egl;

    egl_destroy_framebuffer(egl, test->fb);
    egl_cleanup(egl);
}

static void
clear_test_draw(struct clear_test *test)
{
    struct egl_gl *gl = &test->egl.gl;

    /* no need to set viewport */
    gl->BindFramebuffer(GL_FRAMEBUFFER, test->fb->fbo);

    gl->ClearColor(test->color[0], test->color[1], test->color[2], test->color[3]);
    gl->Clear(GL_COLOR_BUFFER_BIT);
    egl_check(&test->egl, "clear");

    egl_dump_image(&test->egl, test->width, test->height, "rt.ppm");

    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

int
main(void)
{
    struct clear_test test = {
        .width = 320,
        .height = 240,
        .color = { 1.0f, 0.0f, 0.0f, 1.0f },
    };

    clear_test_init(&test);
    clear_test_draw(&test);
    clear_test_cleanup(&test);

    return 0;
}
