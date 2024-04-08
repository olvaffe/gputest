/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "clutil.h"

static const char bench_copy_cs[] = "                    \n\
kernel void memcpy32(global uint *dst, global uint *src) \n\
{                                                        \n\
    size_t idx = get_global_id(0);                       \n\
    dst[idx] = src[idx];                                 \n\
}";

struct bench_copy {
    size_t size;

    struct cl cl;

    struct cl_buffer *src;
    struct cl_buffer *dst;

    struct cl_pipeline *pipeline;
};

static void
bench_copy_init_size(struct bench_copy *test)
{
    struct cl *cl = &test->cl;

    if (!test->size) {
        test->size = cl->dev->max_mem_alloc_size;

        const size_t gb = 1024u * 1024 * 1024;
        if (test->size > gb)
            test->size = gb;

        test->size = ALIGN(test->size, sizeof(cl_uint16));
    }

    if (test->size % sizeof(cl_uint16))
        cl_die("size is not uint16-aligned");
}

static void
bench_copy_init_buffers(struct bench_copy *test)
{
    struct cl *cl = &test->cl;

    const cl_uint val = 0x12345678;
    test->src = cl_create_buffer(cl, CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS, test->size);
    cl_fill_buffer(cl, test->src, &val, sizeof(val));

    test->dst = cl_create_buffer(cl, CL_MEM_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS, test->size);
}

static void
bench_copy_init(struct bench_copy *test)
{
    struct cl *cl = &test->cl;

    const struct cl_init_params params = {
        .profiling = true,
    };
    cl_init(cl, &params);

    bench_copy_init_size(test);
    bench_copy_init_buffers(test);

    test->pipeline = cl_create_pipeline(cl, bench_copy_cs, "memcpy32");
}

static void
bench_copy_cleanup(struct bench_copy *test)
{
    struct cl *cl = &test->cl;

    cl_destroy_pipeline(cl, test->pipeline);

    cl_destroy_buffer(cl, test->dst);
    cl_destroy_buffer(cl, test->src);

    cl_cleanup(cl);
}

static void
bench_copy_dispatch(struct bench_copy *test)
{
    struct cl *cl = &test->cl;
    const size_t count = test->size / sizeof(cl_uint);
    const uint32_t loops = 5;

    cl_set_pipeline_arg(cl, test->pipeline, 0, &test->dst->mem, sizeof(test->dst->mem));
    cl_set_pipeline_arg(cl, test->pipeline, 1, &test->src->mem, sizeof(test->src->mem));

    for (uint32_t i = 0; i < loops; i++) {
        cl_event ev;

        cl_enqueue_pipeline(cl, test->pipeline, count, 0, 0, &ev);
        cl_wait_event(cl, ev);

        cl_ulong start_ns;
        cl_ulong end_ns;
        cl_get_event_profiling_info(cl, ev, CL_PROFILING_COMMAND_START, &start_ns,
                                    sizeof(start_ns));
        cl_get_event_profiling_info(cl, ev, CL_PROFILING_COMMAND_END, &end_ns, sizeof(end_ns));
        const uint32_t dur_us = (end_ns - start_ns) / 1000;
        const float gbps = (float)test->size / (end_ns - start_ns) / 1.024f / 1.024f / 1.024f;
        cl_log("copying %zu MiBs took %.3f ms: %.1f GiB/s", test->size / 1024 / 1024,
               (float)dur_us / 1000.0f, gbps);

        cl_destroy_event(cl, ev);
    }
}

int
main(void)
{
    struct bench_copy test = {
        .size = 0,
    };

    bench_copy_init(&test);
    bench_copy_dispatch(&test);
    bench_copy_cleanup(&test);

    return 0;
}
