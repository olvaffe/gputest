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

static const float model_test_mvp[16] = {
    1.0f, 0.0f, 0.0f, 0.0f, /* col 0 */
    0.0f, 1.0f, 0.0f, 0.0f, /* col 1 */
    0.0f, 0.0f, 1.0f, 0.0f, /* col 2 */
    0.0f, 0.0f, 0.0f, 1.0f, /* col 3 */
};

struct model {
    float *vertices;
    int vertex_count;

    int *faces;
    int face_count;

    GLsizei vertex_stride;
    GLint attr_sizes[3];
    GLenum attr_types[3];
    GLsizei attr_offsets[3];

    GLuint vbo;
    GLuint ibo;
};

struct model_test {
    uint32_t width;
    uint32_t height;
    int loop;

    const char *filename;

    struct rdoc rdoc;
    struct egl egl;
    struct egl_framebuffer *fb;

    struct egl_program *prog;
    GLuint prog_bones;
    GLuint prog_mvp;

    struct egl_stopwatch *stopwatch;
    struct model model;
};

static void
model_test_get_bounding_box(struct model_test *test, float min[3], float max[3])
{
    const struct model *model = &test->model;

    if (!model->vertex_count) {
        memset(min, 0, sizeof(*min) * 3);
        memset(max, 0, sizeof(*max) * 3);
        return;
    }

    min[0] = model->vertices[0];
    min[1] = model->vertices[1];
    min[2] = model->vertices[2];
    max[0] = model->vertices[0];
    max[1] = model->vertices[1];
    max[2] = model->vertices[2];
    for (int i = 1; i < model->vertex_count; i++) {
        const float *vert = &model->vertices[i * 3];

        for (int dim = 0; dim < 3; dim++) {
            if (max[dim] < vert[dim])
                max[dim] = vert[dim];
            if (min[dim] > vert[dim])
                min[dim] = vert[dim];
        }
    }
}

static void
model_test_upload_model(struct model_test *test)
{
    struct model *model = &test->model;
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    float min[3];
    float max[3];
    model_test_get_bounding_box(test, min, max);

    const float center[3] = {
        (max[0] + min[0]) / 2.0f,
        (max[1] + min[1]) / 2.0f,
        (max[2] + min[2]) / 2.0f,
    };

    float extent = max[0] - min[0];
    if (extent < max[1] - min[1])
        extent = max[1] - min[1];
    if (extent < max[2] - min[2])
        extent = max[2] - min[2];

    /* translate bounding box to origin */
    const float xlate[3] = {
        -center[0],
        -center[1],
        -center[2],
    };
    /* scale bounding box to [-1.0, 1.0] */
    const float scale = 2.0f / extent;

    for (int i = 0; i < model->vertex_count; i++) {
        float *vert = &model->vertices[i * 3];
        for (int dim = 0; dim < 3; dim++)
            vert[dim] = (vert[dim] + xlate[dim]) * scale;
    }

    for (int i = 0; i < model->face_count; i++) {
        int *face = &model->faces[i * 3];
        face[0] -= 1;
        face[1] -= 1;
        face[2] -= 1;
    }

    struct vert {
        float pos[3];
        float pad1[2];
        float bone_weights[4];
        uint8_t bone_indices[4];
        float pad2[2];
    };

    model->vertex_stride = sizeof(struct vert);
    model->attr_sizes[0] = 3;
    model->attr_offsets[0] = offsetof(struct vert, pos);
    model->attr_types[0] = GL_FLOAT;
    model->attr_sizes[1] = 4;
    model->attr_offsets[1] = offsetof(struct vert, bone_indices);
    model->attr_types[1] = GL_UNSIGNED_BYTE;
    model->attr_sizes[2] = 4;
    model->attr_types[2] = GL_FLOAT;
    model->attr_offsets[2] = offsetof(struct vert, bone_weights);

    struct vert *verts = calloc(model->vertex_count, sizeof(struct vert));
    if (!verts)
        egl_die("failed to alloc verts");
    for (int i = 0; i < model->vertex_count; i++) {
        struct vert *vert = &verts[i];

        memcpy(vert->pos, &model->vertices[i * 3], sizeof(vert->pos));
        for (int j = 0; j < 4; j++) {
            vert->bone_weights[j] = 0.25f;
            vert->bone_indices[j] = (i * 4 + j) % 32;
        }
    }

    const GLsizeiptr vbo_size = model->vertex_stride * model->vertex_count;
    gl->GenBuffers(1, &model->vbo);
    gl->BindBuffer(GL_ARRAY_BUFFER, model->vbo);
    gl->BufferData(GL_ARRAY_BUFFER, vbo_size, verts, GL_STATIC_DRAW);

    free(verts);

    const GLsizeiptr ibo_size = sizeof(*model->faces) * model->face_count * 3;
    gl->GenBuffers(1, &model->ibo);
    gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->ibo);
    gl->BufferData(GL_ELEMENT_ARRAY_BUFFER, ibo_size, model->faces, GL_STATIC_DRAW);

    free(model->vertices);
    free(model->faces);
    model->vertices = NULL;
    model->faces = NULL;
}

static void
model_test_parse_model(struct model_test *test, const char *ptr, size_t size)
{
    struct model *model = &test->model;

    while (size) {
        const char *line = ptr;
        const char *end = strchr(ptr, '\n');
        if (!end)
            break;

        bool parsed = true;
        if (!strncmp(line, "v ", 2)) {
            if (model->vertices) {
                float *vert = &model->vertices[model->vertex_count * 3];
                if (sscanf(line, "v %f %f %f", &vert[0], &vert[1], &vert[2]) != 3)
                    parsed = false;
            }
            model->vertex_count++;
        } else if (!strncmp(line, "f ", 2)) {
            if (model->faces) {
                int *face = &model->faces[model->face_count * 3];
                if (sscanf(line, "f %d %d %d", &face[0], &face[1], &face[2]) != 3)
                    parsed = false;
            }
            model->face_count++;
        } else {
            parsed = false;
        }

        if (!parsed)
            egl_die("unsupported line: %.*s", (int)(end - line), line);

        ptr = end + 1;
        size -= end - line + 1;
    }
}

static void
model_test_init_model(struct model_test *test)
{
    size_t size;
    const char *ptr = u_map_file(test->filename, &size);
    if (!ptr)
        egl_die("failed to map %s", test->filename);

    model_test_parse_model(test, ptr, size);

    test->model.vertices = malloc(sizeof(*test->model.vertices) * test->model.vertex_count * 3);
    if (!test->model.vertices)
        egl_die("failed to alloc vertices");

    test->model.faces = malloc(sizeof(*test->model.faces) * test->model.face_count * 3);
    if (!test->model.faces)
        egl_die("failed to alloc faces");

    test->model.vertex_count = 0;
    test->model.face_count = 0;
    model_test_parse_model(test, ptr, size);

    u_unmap_file(ptr, size);

    model_test_upload_model(test);
}

static void
model_test_init(struct model_test *test)
{
    struct rdoc *rdoc = &test->rdoc;
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    rdoc_init(rdoc);
    egl_init(egl, NULL);
    test->fb =
        egl_create_framebuffer(egl, test->width, test->height, GL_RGB8, GL_DEPTH_COMPONENT16);

    test->prog = egl_create_program(egl, model_test_vs, model_test_fs);
    test->prog_bones = gl->GetUniformLocation(test->prog->prog, "bones");
    test->prog_mvp = gl->GetUniformLocation(test->prog->prog, "mvp");

    test->stopwatch = egl_create_stopwatch(egl, test->loop * 2);

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

    egl_destroy_stopwatch(egl, test->stopwatch);
    egl_destroy_program(egl, test->prog);
    egl_destroy_framebuffer(egl, test->fb);

    gl->DeleteBuffers(1, &test->model.vbo);
    gl->DeleteBuffers(1, &test->model.ibo);

    egl_cleanup(egl);
    rdoc_cleanup(rdoc);
}

static void
model_test_draw(struct model_test *test)
{
    struct rdoc *rdoc = &test->rdoc;
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    rdoc_start(rdoc);

    gl->BindFramebuffer(GL_FRAMEBUFFER, test->fb->fbo);
    gl->Viewport(1, 1, test->width - 2, test->height - 2);

    gl->Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    egl_check(egl, "clear");

    gl->UseProgram(test->prog->prog);

    /* identity */
    float bones[32 * 3 * 4];
    for (uint32_t i = 0; i < 32; i++) {
        for (uint32_t j = 0; j < 3; j++) {
            for (uint32_t k = 0; k < 4; k++) {
                const float val = j == k ? 1.0f : 0.0f;
                bones[12 * i + 4 * j + k] = val;
            }
        }
    }
    gl->Uniform4fv(test->prog_bones, ARRAY_SIZE(bones) / 4, bones);
    gl->UniformMatrix4fv(test->prog_mvp, 1, false, model_test_mvp);

    gl->Enable(GL_CULL_FACE);
    gl->Enable(GL_DEPTH_TEST);

    for (int i = 0; i < 3; i++) {
        gl->VertexAttribPointer(i, test->model.attr_sizes[i], test->model.attr_types[i], GL_FALSE,
                                test->model.vertex_stride,
                                (const void *)(intptr_t)test->model.attr_offsets[i]);
        gl->EnableVertexAttribArray(i);
    }

    egl_check(egl, "setup");

    for (int i = 0; i < test->loop; i++) {
        egl_write_stopwatch(egl, test->stopwatch);
        gl->DrawElements(GL_TRIANGLES, test->model.face_count * 3, GL_UNSIGNED_INT,
                         test->model.faces);
        egl_write_stopwatch(egl, test->stopwatch);
    }
    egl_check(egl, "draw");

    gl->Finish();

    uint64_t gpu_ns = 0;
    for (int i = 0; i < test->loop; i++)
        gpu_ns += egl_read_stopwatch(egl, test->stopwatch, i * 2);
    const int gpu_us = (int)(gpu_ns / 1000);
    egl_log("gpu time: %d.%dms", gpu_us / 1000, gpu_us % 1000);

    egl_dump_image(&test->egl, test->width, test->height, "rt.ppm");

    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);

    rdoc_end(rdoc);
}

int
main(int argc, const char **argv)
{
    struct model_test test = {
        .width = 1024,
        .height = 1024,
        .loop = 20,
    };

    if (argc != 2)
        egl_die("usage: %s <obj>", argv[0]);

    test.filename = argv[1];

    model_test_init(&test);
    model_test_draw(&test);
    model_test_cleanup(&test);

    return 0;
}
