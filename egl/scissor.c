/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "eglutil.h"

struct scissor_test {
    uint32_t width;
    uint32_t height;
    float color1[4];
    float color2[4];

    struct egl egl;
    struct egl_framebuffer *fb;
};

static void
scissor_test_init(struct scissor_test *test)
{
    struct egl *egl = &test->egl;

    egl_init(egl, NULL);
    test->fb = egl_create_framebuffer(egl, test->width, test->height, GL_RGBA8, GL_NONE);
}

static void
scissor_test_cleanup(struct scissor_test *test)
{
    struct egl *egl = &test->egl;

    egl_destroy_framebuffer(egl, test->fb);
    egl_cleanup(egl);
}

static void
scissor_test_draw(struct scissor_test *test)
{
    struct egl_gl *gl = &test->egl.gl;

    /* no need to set viewport */
    gl->BindFramebuffer(GL_FRAMEBUFFER, test->fb->fbo);

    gl->Enable(GL_SCISSOR_TEST);
    for (uint32_t i = 0; i < test->height; i++) {
        const float *color = i % 2 ? test->color1 : test->color2;
        gl->ClearColor(color[0], color[1], color[2], color[3]);

        gl->Scissor(0, i, test->width, 1);
        gl->Clear(GL_COLOR_BUFFER_BIT);
    }
    egl_check(&test->egl, "clear");

    egl_dump_image(&test->egl, test->width, test->height, "rt.ppm");

    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

int
main(void)
{
    struct scissor_test test = {
        .width = 320,
        .height = 240,
        .color1 = { 1.0f, 1.0f, 1.0f, 1.0f },
        .color2 = { 0.0f, 0.0f, 0.0f, 1.0f },
    };

    scissor_test_init(&test);
    scissor_test_draw(&test);
    scissor_test_cleanup(&test);

    return 0;
}
