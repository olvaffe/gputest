/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "androidutil.h"
#include "eglutil.h"

struct ahb_image_test {
    AHardwareBuffer_Desc desc;

    struct android android;
    struct egl egl;
    bool EXT_EGL_image_storage;

    struct android_ahb *ahb;
    struct egl_image *img;

    GLenum tex_target;
    GLuint tex;
};

static void
ahb_image_test_init_texture(struct ahb_image_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    test->tex_target = GL_TEXTURE_2D;
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
ahb_image_test_init_image(struct ahb_image_test *test)
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
ahb_image_test_init_ahb(struct ahb_image_test *test)
{
    struct android *android = &test->android;

    test->ahb = android_create_ahb(android, test->desc.width, test->desc.height,
                                   test->desc.format, test->desc.usage);
}

static void
ahb_image_test_init(struct ahb_image_test *test)
{
    struct android *android = &test->android;
    struct egl *egl = &test->egl;

    android_init(android, NULL);
    egl_init(egl, NULL);

    if (!egl->ANDROID_get_native_client_buffer)
        egl_die("no EGL_ANDROID_get_native_client_buffer");
    if (!egl->ANDROID_image_native_buffer)
        egl_die("no EGL_ANDROID_image_native_buffer");

    test->EXT_EGL_image_storage = strstr(egl->gl_exts, "GL_EXT_EGL_image_storage");

    ahb_image_test_init_ahb(test);
    ahb_image_test_init_image(test);
    ahb_image_test_init_texture(test);

    egl_check(egl, "init");
}

static void
ahb_image_test_cleanup(struct ahb_image_test *test)
{
    struct android *android = &test->android;
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    egl_check(egl, "cleanup");

    gl->DeleteTextures(1, &test->tex);

    egl_destroy_image(egl, test->img);
    android_destroy_ahb(android, test->ahb);

    egl_cleanup(egl);
    android_cleanup(android);
}

static void
ahb_image_test_draw(struct ahb_image_test *test)
{
}

int
main(int argc, const char **argv)
{
    struct ahb_image_test test = {
        .desc = {
            .format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
            .height = 16,
            .layers = 1,
            .usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                     AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY,
            .width = 16,
        },
    };

    ahb_image_test_init(&test);
    ahb_image_test_draw(&test);
    ahb_image_test_cleanup(&test);

    return 0;
}
