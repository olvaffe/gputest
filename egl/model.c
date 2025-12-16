/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "eglutil.h"
#include "rdocutil.h"

static const char model_test_vs[] = {
#include "model_test.vert.inc"
};

static const char model_test_fs[] = {
#include "model_test.frag.inc"
};

struct model {
    float (*v)[3];
    int v_count;

    int (*f)[3];
    int f_count;

    GLuint vbo;
    GLsizei vertex_stride;
    struct {
        GLint size;
        GLenum type;
        GLsizei offset;
    } attrs[3];

    GLuint ibo;
    struct {
        GLenum mode;
        GLsizei count;
        GLenum type;
    } elem;
};

struct model_test {
    uint32_t width;
    uint32_t height;
    GLenum rt_format;
    GLenum ds_format;
    bool depth_test;
    bool normalize_model;
    int outer_loop;
    int inner_loop;

    const char *filename;

    struct rdoc rdoc;

    struct egl egl;
    struct egl_framebuffer *fb;

    struct egl_program *prog;
    GLuint loc_bones;
    GLuint loc_mvp;

    struct egl_stopwatch *stopwatch;

    struct model model;
};

static void
model_test_upload_model(struct model_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;
    struct model *model = &test->model;

    struct hwvert {
        float pos[3];
        float pad1[2];
        float bone_weight[4];
        uint8_t bone_index[4];
        float pad2[2];
    };

    struct hwvert *hwverts = calloc(model->v_count, sizeof(*hwverts));
    if (!hwverts)
        egl_die("failed to alloc hwverts");
    for (int i = 0; i < model->v_count; i++) {
        struct hwvert *hwvert = &hwverts[i];

        memcpy(hwvert->pos, model->v[i], sizeof(hwvert->pos));
        for (int j = 0; j < 4; j++) {
            hwvert->bone_weight[j] = 0.25f;
            hwvert->bone_index[j] = (i * 4 + j) % 32;
        }
    }

    const GLsizeiptr vbo_size = sizeof(*hwverts) * model->v_count;
    gl->GenBuffers(1, &model->vbo);
    gl->BindBuffer(GL_ARRAY_BUFFER, model->vbo);
    gl->BufferData(GL_ARRAY_BUFFER, vbo_size, hwverts, GL_STATIC_DRAW);
    gl->BindBuffer(GL_ARRAY_BUFFER, 0);

    free(hwverts);

    model->vertex_stride = sizeof(struct hwvert);
    model->attrs[0].size = 3;
    model->attrs[0].type = GL_FLOAT;
    model->attrs[0].offset = offsetof(struct hwvert, pos);
    model->attrs[1].size = 4;
    model->attrs[1].type = GL_UNSIGNED_BYTE;
    model->attrs[1].offset = offsetof(struct hwvert, bone_index);
    model->attrs[2].size = 4;
    model->attrs[2].type = GL_FLOAT;
    model->attrs[2].offset = offsetof(struct hwvert, bone_weight);

    const GLsizeiptr ibo_size = sizeof(*model->f) * model->f_count;
    gl->GenBuffers(1, &model->ibo);
    gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->ibo);
    gl->BufferData(GL_ELEMENT_ARRAY_BUFFER, ibo_size, model->f, GL_STATIC_DRAW);
    gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    model->elem.mode = GL_TRIANGLES;
    model->elem.count = model->f_count * 3;
    model->elem.type = GL_UNSIGNED_INT;

    free(model->v);
    free(model->f);
    model->v = NULL;
    model->f = NULL;
}

static void
model_test_process_model(struct model_test *test)
{
    struct model *model = &test->model;

    if (test->normalize_model && model->v_count) {
        /* find bounding box */
        float min[3];
        float max[3];
        for (int dim = 0; dim < 3; dim++) {
            const float *v = model->v[0];
            min[dim] = v[dim];
            max[dim] = v[dim];
        }
        for (int i = 1; i < model->v_count; i++) {
            const float *v = model->v[i];

            for (int dim = 0; dim < 3; dim++) {
                if (max[dim] < v[dim])
                    max[dim] = v[dim];
                if (min[dim] > v[dim])
                    min[dim] = v[dim];
            }
        }

        float center[3];
        float extent = 0.0f;
        for (int dim = 0; dim < 3; dim++) {
            center[dim] = (max[dim] + min[dim]) / 2.0f;

            const float val = max[dim] - min[dim];
            if (extent < val)
                extent = val;
        }

        /* translate bounding box to origin */
        const float xlate[3] = {
            -center[0],
            -center[1],
            -center[2],
        };
        /* scale bounding box to [-1.0, 1.0] */
        const float scale = 2.0f / extent;

        for (int i = 0; i < model->v_count; i++) {
            float *v = model->v[i];
            for (int dim = 0; dim < 3; dim++)
                v[dim] = (v[dim] + xlate[dim]) * scale;
        }
    }

    for (int i = 0; i < model->f_count; i++) {
        int *f = model->f[i];
        /* zero-based */
        f[0] -= 1;
        f[1] -= 1;
        f[2] -= 1;
    }
}

static void
model_test_parse_model(struct model_test *test, const char *ptr, size_t size)
{
    struct model *model = &test->model;
    const char *line = ptr;
    const char *end = ptr + size;

    while (line < end) {
        const char *newline = memchr(line, '\n', end - line);
        if (!newline)
            break;

        bool parsed = true;
        if (!strncmp(line, "v ", 2)) {
            if (model->v) {
                float *v = model->v[model->v_count];
                if (sscanf(line, "v %f %f %f", &v[0], &v[1], &v[2]) != 3)
                    parsed = false;
            }
            model->v_count++;
        } else if (!strncmp(line, "f ", 2)) {
            if (model->f) {
                int *f = model->f[model->f_count];
                if (sscanf(line, "f %d %d %d", &f[0], &f[1], &f[2]) != 3)
                    parsed = false;
            }
            model->f_count++;
        } else {
            parsed = false;
        }

        if (!parsed)
            egl_die("unsupported line: %.*s", (int)(newline - line), line);

        line = newline + 1;
    }
}

static void
model_test_init_model(struct model_test *test)
{
    struct model *model = &test->model;

    size_t size;
    const char *ptr = u_map_file(test->filename, &size);
    if (!ptr)
        egl_die("failed to map %s", test->filename);

    model_test_parse_model(test, ptr, size);

    model->v = malloc(sizeof(*model->v) * model->v_count);
    if (!model->v)
        egl_die("failed to alloc v");

    model->f = malloc(sizeof(*model->f) * model->f_count);
    if (!model->f)
        egl_die("failed to alloc f");

    model->v_count = 0;
    model->f_count = 0;
    model_test_parse_model(test, ptr, size);

    u_unmap_file(ptr, size);

    model_test_process_model(test);
    model_test_upload_model(test);
}

static void
model_test_init_program(struct model_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    test->prog = egl_create_program(egl, model_test_vs, model_test_fs);
    test->loc_bones = gl->GetUniformLocation(test->prog->prog, "bones");
    test->loc_mvp = gl->GetUniformLocation(test->prog->prog, "mvp");

    /* identity */
    float bones[32 * 3][4];
    for (uint32_t i = 0; i < 32; i++) {
        float (*bone)[4] = &bones[i * 3];
        for (uint32_t j = 0; j < 3; j++) {
            float *col = bone[j];
            for (uint32_t k = 0; k < 4; k++) {
                const float val = j == k ? 1.0f : 0.0f;
                col[k] = val;
            }
        }
    }

    const float mvp[16] = {
        1.0f, 0.0f, 0.0f, 0.0f, /* col 0 */
        0.0f, 1.0f, 0.0f, 0.0f, /* col 1 */
        0.0f, 0.0f, 1.0f, 0.0f, /* col 2 */
        0.0f, 0.0f, 0.0f, 1.0f, /* col 3 */
    };

    gl->UseProgram(test->prog->prog);
    gl->Uniform4fv(test->loc_bones, ARRAY_SIZE(bones), (const float *)bones);
    gl->UniformMatrix4fv(test->loc_mvp, 1, false, mvp);
    gl->UseProgram(0);
}

static void
model_test_init_framebuffer(struct model_test *test)
{
    struct egl *egl = &test->egl;

    test->fb =
        egl_create_framebuffer(egl, test->width, test->height, test->rt_format, test->ds_format);
}

static void
model_test_init(struct model_test *test)
{
    struct rdoc *rdoc = &test->rdoc;
    struct egl *egl = &test->egl;

    rdoc_init(rdoc);

    egl_init(egl, NULL);

    model_test_init_framebuffer(test);
    model_test_init_program(test);
    test->stopwatch = egl_create_stopwatch(egl, test->outer_loop * 2);

    model_test_init_model(test);

    egl_check(egl, "init");
}

static void
model_test_cleanup(struct model_test *test)
{
    struct rdoc *rdoc = &test->rdoc;
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    egl_check(egl, "cleanup");

    gl->DeleteBuffers(1, &test->model.vbo);
    gl->DeleteBuffers(1, &test->model.ibo);

    egl_destroy_stopwatch(egl, test->stopwatch);
    egl_destroy_program(egl, test->prog);
    egl_destroy_framebuffer(egl, test->fb);

    egl_cleanup(egl);

    rdoc_cleanup(rdoc);
}

static void
model_test_draw_model(struct model_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;
    const struct model *model = &test->model;

    gl->Enable(GL_CULL_FACE);
    if (test->depth_test)
        gl->Enable(GL_DEPTH_TEST);

    gl->UseProgram(test->prog->prog);

    gl->BindBuffer(GL_ARRAY_BUFFER, model->vbo);
    gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->ibo);
    for (int i = 0; i < (int)ARRAY_SIZE(model->attrs); i++) {
        gl->VertexAttribPointer(i, model->attrs[i].size, model->attrs[i].type, GL_FALSE,
                                model->vertex_stride,
                                (const void *)(intptr_t)model->attrs[i].offset);
        gl->EnableVertexAttribArray(i);
    }

    egl_check(egl, "setup");

    for (int i = 0; i < test->inner_loop; i++) {
        gl->DrawElements(model->elem.mode, model->elem.count, model->elem.type, NULL);
    }
    egl_check(egl, "draw");
}

static void
model_test_draw(struct model_test *test)
{
    struct rdoc *rdoc = &test->rdoc;
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    rdoc_start(rdoc);

    for (int i = 0; i < test->outer_loop; i++) {
        const GLbitfield clear_mask =
            GL_COLOR_BUFFER_BIT | (test->depth_test ? GL_DEPTH_BUFFER_BIT : 0);

        gl->BindFramebuffer(GL_FRAMEBUFFER, test->fb->fbo);

        gl->ClearColor(1.0, 1.0, 1.0, 1.0);
        gl->Clear(clear_mask);
        egl_check(egl, "clear");

        gl->Viewport(1, 1, test->width - 2, test->height - 2);

        egl_write_stopwatch(egl, test->stopwatch);
        model_test_draw_model(test);
        egl_write_stopwatch(egl, test->stopwatch);
    }

    gl->Finish();

    for (int i = 0; i < test->outer_loop; i++) {
        const uint64_t gpu_ns = egl_read_stopwatch(egl, test->stopwatch, i * 2);
        const int gpu_us = (int)(gpu_ns / 1000);
        egl_log("gpu time: %d.%dms", gpu_us / 1000, gpu_us % 1000);
    }

    egl_dump_image(&test->egl, test->width, test->height, "rt.ppm");

    rdoc_end(rdoc);
}

int
main(int argc, const char **argv)
{
    struct model_test test = {
        .width = 1024,
        .height = 1024,
        .rt_format = GL_RGB8,
        .ds_format = GL_DEPTH_COMPONENT16,
        .depth_test = true,
        .normalize_model = false,
        .outer_loop = 20,
        .inner_loop = 1,
    };

    if (argc < 2 || argc > 3)
        egl_die("usage: %s <obj> [<depth-test>]", argv[0]);

    test.filename = argv[1];
    if (argc >= 3)
        test.depth_test = atoi(argv[2]);

    if (!test.depth_test)
        test.ds_format = GL_NONE;

    model_test_init(&test);
    model_test_draw(&test);
    model_test_cleanup(&test);

    return 0;
}
