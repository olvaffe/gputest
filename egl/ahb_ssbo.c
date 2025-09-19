/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "androidutil.h"
#include "eglutil.h"

static const char ahb_ssbo_test_cs[] = {
#include "ahb_ssbo_test.comp.inc"
};

struct ahb_ssbo_test {
    uint32_t local_size;

    struct android android;
    struct egl egl;

    struct android_ahb *ahb;
    uint32_t ahb_size;

    GLuint ssbo;
    GLuint shader;
    GLuint prog;
};

static void
ahb_ssbo_test_init_program(struct ahb_ssbo_test *test)
{
    struct egl *egl = &test->egl;

    test->shader = egl_compile_shader(egl, GL_COMPUTE_SHADER, ahb_ssbo_test_cs),
    test->prog = egl_link_program(egl, &test->shader, 1);
}

static void
ahb_ssbo_test_init_ssbo(struct ahb_ssbo_test *test)
{
    const GLbitfield ssbo_flags = 0;
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    gl->GenBuffers(1, &test->ssbo);
    gl->BindBuffer(GL_SHADER_STORAGE_BUFFER, test->ssbo);

    EGLClientBuffer buf = (GLeglClientBufferEXT)egl->GetNativeClientBufferANDROID(test->ahb->ahb);
    gl->BufferStorageExternalEXT(GL_SHADER_STORAGE_BUFFER, 0, test->ahb_size, buf, ssbo_flags);

    if (ssbo_flags & GL_DYNAMIC_STORAGE_BIT_EXT) {
        void *init_vals = malloc(test->ahb_size);
        memset(init_vals, 0xff, test->ahb_size);
        gl->BufferSubData(GL_SHADER_STORAGE_BUFFER, 0, test->ahb_size, init_vals);
        free(init_vals);
    }
}

static void
ahb_ssbo_test_init_ahb(struct ahb_ssbo_test *test)
{
    struct android *android = &test->android;

    test->ahb_size = test->local_size * sizeof(uint32_t);
    test->ahb = android_create_ahb(
        android, test->ahb_size, 1, AHARDWAREBUFFER_FORMAT_BLOB,
        AHARDWAREBUFFER_USAGE_CPU_READ_RARELY | AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER);
}

static void
ahb_ssbo_test_init(struct ahb_ssbo_test *test)
{
    struct android *android = &test->android;
    struct egl *egl = &test->egl;

    android_init(android, NULL);
    egl_init(egl, NULL);

    if (!egl->ANDROID_get_native_client_buffer)
        egl_die("no EGL_ANDROID_get_native_client_buffer");
    if (!strstr(egl->gl_exts, "GL_EXT_buffer_storage"))
        egl_die("no GL_EXT_buffer_storage");
    if (!strstr(egl->gl_exts, "GL_EXT_external_buffer"))
        egl_die("no GL_EXT_external_buffer");

    ahb_ssbo_test_init_ahb(test);
    ahb_ssbo_test_init_ssbo(test);
    ahb_ssbo_test_init_program(test);

    egl_check(egl, "init");
}

static void
ahb_ssbo_test_cleanup(struct ahb_ssbo_test *test)
{
    struct android *android = &test->android;
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    egl_check(egl, "cleanup");

    gl->DeleteProgram(test->prog);
    gl->DeleteShader(test->shader);

    gl->DeleteBuffers(1, &test->ssbo);

    android_destroy_ahb(android, test->ahb);

    egl_cleanup(egl);
    android_cleanup(android);
}

static void
ahb_ssbo_test_draw(struct ahb_ssbo_test *test)
{
    struct android *android = &test->android;
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    gl->BindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, test->ssbo);
    gl->UseProgram(test->prog);
    egl_check(egl, "setup");

    gl->DispatchCompute(1, 1, 1);
    egl_check(egl, "compute");

    /* memory barrier? */
    gl->Finish();
    egl_check(egl, "finish");

    AHardwareBuffer_Planes planes;
    android_map_ahb(android, test->ahb, &planes);
    const uint32_t *vals = planes.planes[0].data;

    for (uint32_t i = 0; i < test->local_size; i++) {
        if (vals[i] != i)
            egl_die("index %d is %d, not %d", i, vals[i], i);
    }

    android_unmap_ahb(android, test->ahb);

    egl_check(egl, "validation");
}

int
main(int argc, const char **argv)
{
    struct ahb_ssbo_test test = {
        .local_size = 64,
    };

    ahb_ssbo_test_init(&test);
    ahb_ssbo_test_draw(&test);
    ahb_ssbo_test_cleanup(&test);

    return 0;
}
