/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "eglutil.h"

static const char ssbo_test_cs[] = {
#include "ssbo_test.comp.inc"
};

struct ssbo_test {
    uint32_t local_size;

    struct egl egl;
    GLuint ssbo;
    GLsizeiptr ssbo_size;
    GLuint shader;
    GLuint prog;
};

static void
ssbo_test_init_program(struct ssbo_test *test)
{
    struct egl *egl = &test->egl;

    test->shader = egl_compile_shader(egl, GL_COMPUTE_SHADER, ssbo_test_cs),
    test->prog = egl_link_program(egl, &test->shader, 1);
}

static void
ssbo_test_init_ssbo(struct ssbo_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    test->ssbo_size = test->local_size * sizeof(uint32_t);

    gl->GenBuffers(1, &test->ssbo);
    gl->BindBuffer(GL_SHADER_STORAGE_BUFFER, test->ssbo);
    gl->BufferStorageEXT(GL_SHADER_STORAGE_BUFFER, test->ssbo_size, NULL, GL_MAP_READ_BIT);
}

static void
ssbo_test_init(struct ssbo_test *test)
{
    struct egl *egl = &test->egl;

    egl_init(egl, NULL);

    if (!strstr(egl->gl_exts, "GL_EXT_buffer_storage"))
        egl_die("no GL_EXT_buffer_storage");

    ssbo_test_init_ssbo(test);
    ssbo_test_init_program(test);

    egl_check(egl, "init");
}

static void
ssbo_test_cleanup(struct ssbo_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    egl_check(egl, "cleanup");

    gl->DeleteProgram(test->prog);
    gl->DeleteShader(test->shader);

    gl->DeleteBuffers(1, &test->ssbo);

    egl_cleanup(egl);
}

static void
ssbo_test_draw(struct ssbo_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    gl->BindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, test->ssbo);
    gl->UseProgram(test->prog);
    egl_check(egl, "setup");

    gl->DispatchCompute(1, 1, 1);
    egl_check(egl, "compute");

    const uint32_t *vals =
        gl->MapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, test->ssbo_size, GL_MAP_READ_BIT);
    egl_check(egl, "map");

    for (uint32_t i = 0; i < test->local_size; i++) {
        if (vals[i] != i)
            egl_die("index %d is %d, not %d", i, vals[i], i);
    }

    gl->UnmapBuffer(GL_SHADER_STORAGE_BUFFER);
}

int
main(int argc, const char **argv)
{
    struct ssbo_test test = {
        .local_size = 64,
    };

    ssbo_test_init(&test);
    ssbo_test_draw(&test);
    ssbo_test_cleanup(&test);

    return 0;
}
