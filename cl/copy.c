/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "clutil.h"

static const char copy_test_cs[] = "                     \n\
kernel void memcpy32(global uint *dst, global uint *src) \n\
{                                                        \n\
    uint idx = get_global_id(0);                         \n\
    dst[idx] = src[idx];                                 \n\
}";

struct copy_test {
    size_t size;

    struct cl cl;
    struct cl_buffer *src;
    struct cl_buffer *dst;
    struct cl_pipeline *pipeline;
};

static void
copy_test_init(struct copy_test *test)
{
    struct cl *cl = &test->cl;

    cl_init(cl, NULL);

    test->src = cl_create_buffer(cl, CL_MEM_ALLOC_HOST_PTR, test->size);
    test->dst = cl_create_buffer(cl, CL_MEM_ALLOC_HOST_PTR, test->size);
    test->pipeline = cl_create_pipeline(cl, copy_test_cs, "memcpy32");
}

static void
copy_test_cleanup(struct copy_test *test)
{
    struct cl *cl = &test->cl;

    cl_destroy_pipeline(cl, test->pipeline);
    cl_destroy_buffer(cl, test->dst);
    cl_destroy_buffer(cl, test->src);
    cl_cleanup(cl);
}

static void
copy_test_dispatch(struct copy_test *test)
{
    struct cl *cl = &test->cl;
    const size_t count = test->size / sizeof(cl_uint);

    cl_uint *ptr = cl_map_buffer(cl, test->src, CL_MAP_WRITE_INVALIDATE_REGION);
    for (uint32_t i = 0; i < count; i++)
        ptr[i] = i;
    cl_unmap_buffer(cl, test->src);

    cl_set_pipeline_arg(cl, test->pipeline, 0, &test->dst->mem, sizeof(test->dst->mem));
    cl_set_pipeline_arg(cl, test->pipeline, 1, &test->src->mem, sizeof(test->src->mem));

    cl_enqueue_pipeline(cl, test->pipeline, 1, NULL, &count, NULL);

    ptr = cl_map_buffer(cl, test->dst, CL_MAP_READ);
    for (uint32_t i = 0; i < count; i++) {
        if (ptr[i] != i)
            cl_die("ptr[%u] is %u, not %u", i, ptr[i], i);
    }
    cl_unmap_buffer(cl, test->dst);

    cl_finish(cl);
}

int
main(void)
{
    struct copy_test test = {
        .size = 1024 * 1024,
    };

    copy_test_init(&test);
    copy_test_dispatch(&test);
    copy_test_cleanup(&test);

    return 0;
}
