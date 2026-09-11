/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "eglutil.h"

struct pbo_test {
    uint32_t width;
    uint32_t height;
    uint8_t color[4];
    bool use_fbo;

    struct egl egl;
    struct egl_framebuffer *fb;
    GLuint pbo;
};

static void
pbo_test_init(struct pbo_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    if (test->use_fbo) {
        egl_init(egl, NULL);
        test->fb = egl_create_framebuffer(egl, test->width, test->height, GL_RGBA8, GL_NONE);
    } else {
        const struct egl_init_params params = {
            .pbuffer_width = test->width,
            .pbuffer_height = test->height,
        };
        egl_init(egl, &params);
    }

    gl->GenBuffers(1, &test->pbo);
    egl_check(egl, "init");
}

static void
pbo_test_cleanup(struct pbo_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    egl_check(egl, "cleanup");

    gl->DeleteBuffers(1, &test->pbo);
    if (test->use_fbo)
        egl_destroy_framebuffer(egl, test->fb);
    egl_cleanup(egl);
}

static void
pbo_test_draw(struct pbo_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    if (test->use_fbo)
        gl->BindFramebuffer(GL_FRAMEBUFFER, test->fb->fbo);

    /* mimic angle ReadPixelsPBOTest.PackLargeRowLength/ES3_Vulkan
     *
     * When using pbuffer, canCopyWithTransformForReadPixels returns false
     * because of flipped y. angle falls back from vkCmdCopyImageToBuffer to
     * custom compute with ssbo. It can potentially violate
     * maxStorageBufferRange limit.
     */

    gl->ClearColor(test->color[0] / 255.0f, test->color[1] / 255.0f, test->color[2] / 255.0f,
                   test->color[3] / 255.0f);
    gl->Clear(GL_COLOR_BUFFER_BIT);
    egl_check(egl, "clear");

    /* almost 128M pixels which translates to almost 512MB or 4Gb */
    const GLint row_length = 128 * 1024 * 1024 - 4;
    const GLsizeiptr row_length_bytes = row_length * 4;
    const GLsizeiptr buffer_size = row_length_bytes + 4;

    gl->BindBuffer(GL_PIXEL_PACK_BUFFER, test->pbo);
    gl->BufferData(GL_PIXEL_PACK_BUFFER, buffer_size, NULL, GL_STREAM_READ);
    egl_check(egl, "buffer data");

    gl->PixelStorei(GL_PACK_ROW_LENGTH, row_length);
    gl->ReadPixels(0, 0, 1, 2, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    egl_check(egl, "readpixels");

    const uint8_t *row0 =
        gl->MapBufferRange(GL_PIXEL_PACK_BUFFER, 0, buffer_size, GL_MAP_READ_BIT);
    if (!row0)
        egl_die("failed to map PBO");
    const uint8_t *row1 = row0 + row_length_bytes;

    if (memcmp(row0, test->color, 4))
        egl_die("row0 is (%d, %d, %d, %d)", row0[0], row0[1], row0[2], row0[3]);
    if (memcmp(row1, test->color, 4))
        egl_die("row1 is (%d, %d, %d, %d)", row1[0], row1[1], row1[2], row1[3]);

    gl->UnmapBuffer(GL_PIXEL_PACK_BUFFER);
    gl->PixelStorei(GL_PACK_ROW_LENGTH, 0);
    gl->BindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    if (test->use_fbo)
        gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

int
main(int argc, const char **argv)
{
    struct pbo_test test = {
        .width = 8,
        .height = 8,
        .color = { 65, 128, 192, 255 },
        .use_fbo = true,
    };

    setlinebuf(stdout);

    pbo_test_init(&test);
    pbo_test_draw(&test);
    pbo_test_cleanup(&test);

    return 0;
}
