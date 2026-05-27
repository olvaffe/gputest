/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "eglutil.h"

static const char uni_test_vs[] = {
#include "uni_test.vert.inc"
};

static const char uni_test_fs[] = {
#include "uni_test.frag.inc"
};

static const float uni_test_vertices[3][2] = {
    {
        -1.0f, /* x */
        -1.0f, /* y */
    },
    {
        1.0f,
        -1.0f,
    },
    {
        0.0f,
        1.0f,
    },
};

struct uni_test {
    uint32_t width;
    uint32_t height;

    struct egl egl;

    struct egl_framebuffer *fb;
    struct egl_program *prog;

    GLuint loc_cond;
    GLuint loc_val1;
    GLuint loc_val2;
};

static void
uni_test_init(struct uni_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    egl_init(egl, NULL);

    test->fb = egl_create_framebuffer(egl, test->width, test->height, GL_RGBA8, GL_NONE);
    test->prog = egl_create_program(egl, uni_test_vs, uni_test_fs);

    test->loc_cond = gl->GetUniformLocation(test->prog->prog, "cond");
    test->loc_val1 = gl->GetUniformLocation(test->prog->prog, "val1[1]");
    test->loc_val2 = gl->GetUniformLocation(test->prog->prog, "val2[1]");

    egl_check(egl, "init");
}

static void
uni_test_cleanup(struct uni_test *test)
{
    struct egl *egl = &test->egl;

    egl_check(egl, "cleanup");

    egl_destroy_program(egl, test->prog);
    egl_destroy_framebuffer(egl, test->fb);

    egl_cleanup(egl);
}

static void
uni_test_draw(struct uni_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    gl->BindFramebuffer(GL_FRAMEBUFFER, test->fb->fbo);
    gl->Viewport(0, 0, test->width, test->height);

    gl->Clear(GL_COLOR_BUFFER_BIT);
    egl_check(egl, "clear");

    gl->UseProgram(test->prog->prog);

    gl->VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(uni_test_vertices[0]),
                            uni_test_vertices);
    gl->EnableVertexAttribArray(0);

    gl->Uniform1f(test->loc_cond, 1.0f);
    gl->Uniform1f(test->loc_val1, 1.0f);
    gl->Uniform1f(test->loc_val1, 0.0f);

    egl_check(egl, "setup");

    gl->DrawArrays(GL_TRIANGLES, 0, 3);
    egl_check(egl, "draw");

    egl_dump_image(&test->egl, test->width, test->height, "rt.ppm");

    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

int
main(int argc, const char **argv)
{
    struct uni_test test = {
        .width = 480,
        .height = 360,
    };

    uni_test_init(&test);
    uni_test_draw(&test);
    uni_test_cleanup(&test);

    return 0;
}
