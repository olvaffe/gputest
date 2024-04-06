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
    cl_command_queue cmdq;
    cl_mem src;
    cl_mem dst;
    cl_program prog;
    cl_kernel kern;
};

static void
copy_test_init(struct copy_test *test)
{
    struct cl *cl = &test->cl;

    cl_init(cl, NULL);

    test->cmdq = cl_create_command_queue(cl, cl->context);

    test->src = cl_create_buffer(cl, cl->context, CL_MEM_ALLOC_HOST_PTR, test->size, NULL);
    test->dst = cl_create_buffer(cl, cl->context, CL_MEM_ALLOC_HOST_PTR, test->size, NULL);

    test->prog = cl_create_program(cl, cl->context, copy_test_cs);
    test->kern = cl_create_kernel(cl, test->prog, "memcpy32");
}

static void
copy_test_cleanup(struct copy_test *test)
{
    struct cl *cl = &test->cl;

    cl_destroy_kernel(cl, test->kern);
    cl_destroy_program(cl, test->prog);
    cl_destroy_memory(cl, test->dst);
    cl_destroy_memory(cl, test->src);
    cl_destroy_command_queue(cl, test->cmdq);
    cl_cleanup(cl);
}

static void
copy_test_dispatch(struct copy_test *test)
{
    struct cl *cl = &test->cl;
    const size_t count = test->size / sizeof(cl_uint);

    cl_uint *ptr =
        cl_map_buffer(cl, test->cmdq, test->src, CL_MAP_WRITE_INVALIDATE_REGION, test->size);
    for (uint32_t i = 0; i < count; i++)
        ptr[i] = i;
    cl_unmap_memory(cl, test->cmdq, test->src, ptr);

    cl_set_kernel_arg(cl, test->kern, 0, &test->dst, sizeof(test->dst));
    cl_set_kernel_arg(cl, test->kern, 1, &test->src, sizeof(test->src));

    cl_enqueue_kernel(cl, test->cmdq, test->kern, 1, NULL, &count, NULL);

    ptr = cl_map_buffer(cl, test->cmdq, test->dst, CL_MAP_READ, test->size);
    for (uint32_t i = 0; i < count; i++) {
        if (ptr[i] != i)
            cl_die("ptr[%u] is %u, not %u", i, ptr[i], i);
    }
    cl_unmap_memory(cl, test->cmdq, test->dst, ptr);

    cl_finish(cl, test->cmdq);
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
