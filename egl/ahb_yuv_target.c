/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "androidutil.h"
#include "eglutil.h"

static const char ahb_yuv_target_test_vs[] = {
#include "ahb_yuv_target_test.vert.inc"
};

static const char ahb_yuv_target_test_fs[] = {
#include "ahb_yuv_target_test.frag.inc"
};

struct ahb_yuv_target_test {
    AHardwareBuffer_Desc desc;
    float clear_color[4];
    float draw_color[4];

    struct android android;
    struct egl egl;
    bool EXT_EGL_image_storage;

    struct android_ahb *ahb;
    struct egl_image *img;

    GLenum tex_target;
    GLuint tex;

    GLuint fbo_target;
    GLuint fbo;

    struct egl_program *prog;
    GLuint loc_color;
};

static void
ahb_yuv_target_test_init_fbo(struct ahb_yuv_target_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    test->fbo_target = GL_FRAMEBUFFER;
    gl->GenFramebuffers(1, &test->fbo);
    gl->BindFramebuffer(test->fbo_target, test->fbo);

    gl->FramebufferTexture2D(test->fbo_target, GL_COLOR_ATTACHMENT0, test->tex_target, test->tex,
                             0);

    if (gl->CheckFramebufferStatus(test->fbo_target) != GL_FRAMEBUFFER_COMPLETE)
        egl_die("incomplete fbo");
}

static void
ahb_yuv_target_test_init_texture(struct ahb_yuv_target_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    test->tex_target = GL_TEXTURE_EXTERNAL_OES;
    gl->GenTextures(1, &test->tex);
    gl->BindTexture(test->tex_target, test->tex);

    gl->TexParameterf(test->tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameterf(test->tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (test->EXT_EGL_image_storage)
        gl->EGLImageTargetTexStorageEXT(test->tex_target, test->img->img, NULL);
    else
        gl->EGLImageTargetTexture2DOES(test->tex_target, test->img->img);
}

static void
ahb_yuv_target_test_init_image(struct ahb_yuv_target_test *test)
{
    struct egl *egl = &test->egl;

    EGLClientBuffer buf = egl->GetNativeClientBufferANDROID(test->ahb->ahb);
    if (!buf)
        egl_die("failed to get client buffer from ahb");

    const struct egl_image_info img_info = {
        .target = EGL_NATIVE_BUFFER_ANDROID,
        .buf = buf,
    };
    test->img = egl_create_image(egl, &img_info);
}

static void
ahb_yuv_target_test_init_ahb(struct ahb_yuv_target_test *test)
{
    struct android *android = &test->android;

    test->ahb = android_create_ahb(android, test->desc.width, test->desc.height,
                                   test->desc.format, test->desc.usage);
}

static void
ahb_yuv_target_test_init(struct ahb_yuv_target_test *test)
{
    struct android *android = &test->android;
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    android_init(android, NULL);
    egl_init(egl, NULL);

    if (!egl->ANDROID_get_native_client_buffer)
        egl_die("no EGL_ANDROID_get_native_client_buffer");
    if (!egl->ANDROID_image_native_buffer)
        egl_die("no EGL_ANDROID_image_native_buffer");

    test->EXT_EGL_image_storage = strstr(egl->gl_exts, "GL_EXT_EGL_image_storage");
    if (!strstr(egl->gl_exts, "GL_EXT_YUV_target"))
        egl_die("no GL_EXT_YUV_target");

    ahb_yuv_target_test_init_ahb(test);
    ahb_yuv_target_test_init_image(test);
    ahb_yuv_target_test_init_texture(test);
    ahb_yuv_target_test_init_fbo(test);

    test->prog = egl_create_program(egl, ahb_yuv_target_test_vs, ahb_yuv_target_test_fs);
    test->loc_color = gl->GetUniformLocation(test->prog->prog, "color");

    egl_check(egl, "init");
}

static void
ahb_yuv_target_test_cleanup(struct ahb_yuv_target_test *test)
{
    struct android *android = &test->android;
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    egl_check(egl, "cleanup");

    egl_destroy_program(egl, test->prog);

    gl->DeleteFramebuffers(1, &test->fbo);

    gl->DeleteTextures(1, &test->tex);

    egl_destroy_image(egl, test->img);
    android_destroy_ahb(android, test->ahb);

    egl_cleanup(egl);
    android_cleanup(android);
}

static void
ahb_yuv_target_test_draw(struct ahb_yuv_target_test *test)
{
    struct android *android = &test->android;
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    gl->BindFramebuffer(GL_FRAMEBUFFER, test->fbo);
    gl->Viewport(0, 0, test->desc.width, test->desc.height);

    gl->ClearColor(test->clear_color[0], test->clear_color[1], test->clear_color[2],
                   test->clear_color[3]);
    gl->Clear(GL_COLOR_BUFFER_BIT);
    egl_check(egl, "clear");

    gl->UseProgram(test->prog->prog);
    gl->Uniform4fv(test->loc_color, 1, test->draw_color);
    egl_check(egl, "setup");

    gl->DrawArrays(GL_TRIANGLES, 0, 3);
    egl_check(egl, "draw");

    gl->Finish();
    egl_check(&test->egl, "finish");

    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);

    android_dump_ahb(android, test->ahb, "rt.ppm");
}

int
main(int argc, const char **argv)
{
    struct ahb_yuv_target_test test = {
        .desc = {
            .format = AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420,
            .height = 300,
            .layers = 1,
            .usage = AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER |
                     AHARDWAREBUFFER_USAGE_CPU_READ_RARELY,
            .width = 300,
        },
        .clear_color = { 0x41 / 255.0f, 0x53 / 255.0f, 0x65 / 255.0f, 1.0f },
        .draw_color = { 0x71 / 255.0f, 0x83 / 255.0f, 0x95 / 255.0f, 1.0f },
    };

    ahb_yuv_target_test_init(&test);
    ahb_yuv_target_test_draw(&test);
    ahb_yuv_target_test_cleanup(&test);

    return 0;
}
