/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "eglutil.h"
#ifdef __ANDROID__
#include "androidutil.h"
#else
#include "gbmutil.h"
#endif

static const char image_test_vs[] = {
#include "image_test.vert.inc"
};

static const char image_test_fs[] = {
#include "image_test.frag.inc"
};

static const unsigned char image_test_ppm[] = {
#include "image_test.ppm.inc"
};

static const float image_test_vertices[4][5] = {
    {
        -1.0f, /* x */
        -1.0f, /* y */
        0.0f,  /* z */
        0.0f,  /* u */
        0.0f,  /* v */
    },
    {
        1.0f,
        -1.0f,
        0.0f,
        1.0f,
        0.0f,
    },
    {
        -1.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
    },
    {
        1.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
    },
};

static const float image_test_tex_transform[4][4] = {
#if 1
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
#else
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, -0.828125f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.8359375f, 0.0f, 1.0f },
#endif
};

struct image_test {
    uint32_t width;
    uint32_t height;
    bool planar;
    bool nearest;

    struct egl egl;
    struct egl_framebuffer *fb;

    GLenum tex_target;
    GLuint tex;

    struct egl_program *prog;

#ifdef __ANDROID__
    struct android android;
    struct android_ahb *ahb;
#else
    struct gbm gbm;
    struct gbm_bo *bo;
#endif

    struct egl_image *img;
};

static void
image_test_init_image(struct image_test *test)
{
    struct egl *egl = &test->egl;
    const uint32_t drm_format = test->planar ? DRM_FORMAT_NV12 : DRM_FORMAT_ABGR8888;

    egl_log("loading ppm as a %s image", test->planar ? "planar" : "non-planar");

#ifdef __ANDROID__
    struct android *android = &test->android;

    android_init(android, NULL);

    test->ahb = android_create_ahb_from_ppm(
        android, image_test_ppm, sizeof(image_test_ppm),
        android_ahb_format_from_drm_format(drm_format),
        AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE);

    if (!egl->ANDROID_get_native_client_buffer)
        egl_die("no ahb export support");
    EGLClientBuffer buf = egl->GetNativeClientBufferANDROID(test->ahb->ahb);
    if (!buf)
        egl_die("failed to get client buffer from ahb");

    const struct egl_image_info img_info = {
        .target = EGL_NATIVE_BUFFER_ANDROID,
        .buf = buf,
    };
    test->img = egl_create_image(egl, &img_info);
#else
    struct gbm *gbm = &test->gbm;

    const struct gbm_init_params gbm_params = {
        .path = egl_get_drm_render_node(egl),
    };
    gbm_init(gbm, &gbm_params);

    test->bo = gbm_create_bo_from_ppm(gbm, image_test_ppm, sizeof(image_test_ppm), drm_format,
                                      NULL, 0, GBM_BO_USE_LINEAR);

    const struct gbm_bo_info *bo_info = gbm_get_bo_info(gbm, test->bo);
    if (bo_info->disjoint)
        egl_die("unsupported disjoint bo");

    struct gbm_import_fd_modifier_data bo_data;
    gbm_export_bo(gbm, test->bo, &bo_data);

    struct egl_image_info img_info = {
        .target = EGL_LINUX_DMA_BUF_EXT,
        .width = bo_data.width,
        .height = bo_data.height,
        .drm_format = bo_data.format,
        .drm_modifier = bo_data.modifier,
        .mem_plane_count = bo_data.num_fds,
        .dma_buf_fd = bo_data.fds[0],
    };

    if (img_info.mem_plane_count > 4)
        egl_die("unexpected plane count");
    for (int i = 0; i < img_info.mem_plane_count; i++) {
        img_info.offsets[i] = bo_data.offsets[i];
        img_info.pitches[i] = bo_data.strides[i];
    }

    test->img = egl_create_image(egl, &img_info);

    for (uint32_t i = 0; i < bo_data.num_fds; i++)
        close(bo_data.fds[i]);
#endif
}

static void
image_test_cleanup_image(struct image_test *test)
{
    struct egl *egl = &test->egl;

    egl_destroy_image(egl, test->img);

#ifdef __ANDROID__
    struct android *android = &test->android;

    android_destroy_ahb(android, test->ahb);
    android_cleanup(android);
#else
    struct gbm *gbm = &test->gbm;

    gbm_destroy_bo(gbm, test->bo);
    gbm_cleanup(gbm);
#endif
}

static void
image_test_init(struct image_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    egl_init(egl, NULL);
    test->fb = egl_create_framebuffer(egl, test->width, test->height);

    if (!strstr(egl->gl_exts, "GL_OES_EGL_image_external"))
        egl_die("no GL_OES_EGL_image_external");

    test->tex_target = GL_TEXTURE_EXTERNAL_OES;
    gl->GenTextures(1, &test->tex);
    gl->BindTexture(test->tex_target, test->tex);
    gl->TexParameterf(test->tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    egl_log("GL_TEXTURE_MAG_FILTER = %s", test->nearest ? "GL_NEAREST" : "GL_LINEAR");
    gl->TexParameterf(test->tex_target, GL_TEXTURE_MAG_FILTER,
                      test->nearest ? GL_NEAREST : GL_LINEAR);
    gl->TexParameteri(test->tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(test->tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    test->prog = egl_create_program(egl, image_test_vs, image_test_fs);

    image_test_init_image(test);

    gl->EGLImageTargetTexture2DOES(test->tex_target, test->img->img);

    egl_check(egl, "init");
}

static void
image_test_cleanup(struct image_test *test)
{
    struct egl *egl = &test->egl;

    egl_check(egl, "cleanup");

    image_test_cleanup_image(test);

    egl_destroy_program(egl, test->prog);

    egl_destroy_framebuffer(egl, test->fb);
    egl_cleanup(egl);
}

static void
image_test_draw(struct image_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    gl->BindFramebuffer(GL_FRAMEBUFFER, test->fb->fbo);
    gl->Viewport(0, 0, test->width, test->height);

    gl->Clear(GL_COLOR_BUFFER_BIT);
    egl_check(egl, "clear");

    gl->UseProgram(test->prog->prog);
    gl->ActiveTexture(GL_TEXTURE0);
    gl->BindTexture(test->tex_target, test->tex);

    gl->UniformMatrix4fv(0, 1, false, (const float *)image_test_tex_transform);

    gl->VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(image_test_vertices[0]),
                            image_test_vertices);
    gl->EnableVertexAttribArray(0);

    gl->VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(image_test_vertices[0]),
                            &image_test_vertices[0][3]);
    gl->EnableVertexAttribArray(1);

    egl_check(egl, "setup");

    gl->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    egl_check(egl, "draw");

    egl_dump_image(&test->egl, test->width, test->height, "rt.ppm");

    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

int
main(int argc, const char **argv)
{
    struct image_test test = {
        .width = 480,
        .height = 360,
    };

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "planar"))
            test.planar = true;
        else if (!strcmp(argv[i], "nearest"))
            test.nearest = true;
        else
            egl_die("unknown option %s", argv[i]);
    }

    image_test_init(&test);
    image_test_draw(&test);
    image_test_cleanup(&test);

    return 0;
}
