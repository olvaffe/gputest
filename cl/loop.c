/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "clutil.h"

static const char loop_test_cs[] = {
#include "loop_test.cl.inc"
};

struct loop_test {
    uint32_t buf_width;
    uint32_t type_size;
    uint32_t local_size;

    struct cl cl;
    struct cl_buffer *dst;
    struct cl_pipeline *pipeline;
};

static void
loop_test_init(struct loop_test *test)
{
    struct cl *cl = &test->cl;

    cl_init(cl, NULL);

    const size_t buf_size = test->buf_width * test->type_size;
    test->dst = cl_create_buffer(cl, CL_MEM_WRITE_ONLY, buf_size, NULL);
    test->pipeline = cl_create_pipeline(cl, loop_test_cs, "loop");
}

static void
loop_test_cleanup(struct loop_test *test)
{
    struct cl *cl = &test->cl;

    cl_destroy_pipeline(cl, test->pipeline);
    cl_destroy_buffer(cl, test->dst);
    cl_cleanup(cl);
}

static void
loop_test_dispatch(struct loop_test *test)
{
    struct cl *cl = &test->cl;

    cl_set_pipeline_arg(cl, test->pipeline, 0, &test->dst->mem, sizeof(test->dst->mem));

    const cl_uint repeat = 100;
    cl_set_pipeline_arg(cl, test->pipeline, 1, &repeat, sizeof(repeat));

    cl_enqueue_pipeline(cl, test->pipeline, test->buf_width, 0, 0, test->local_size, 0, 0, NULL);
    cl_finish(cl);
}

int
main(void)
{
    struct loop_test test = {
        .buf_width = 64 * 64,
        .type_size = 2 * 1,
        .local_size = 64,
    };

    loop_test_init(&test);
    loop_test_dispatch(&test);
    loop_test_cleanup(&test);

    return 0;
}
