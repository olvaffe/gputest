/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "clutil.h"

static const char bench_fill_cs[] = "            \n\
kernel void memset32(global uint *dst, uint val) \n\
{                                                \n\
    size_t idx = get_global_id(0);               \n\
    dst[idx] = val;                              \n\
}";

struct bench_fill {
    size_t size;
    cl_uint val;

    struct cl cl;

    struct cl_buffer *buf;
    struct cl_pipeline *pipeline;
};

static void
bench_fill_init_size(struct bench_fill *test)
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
bench_fill_init(struct bench_fill *test)
{
    struct cl *cl = &test->cl;

    const struct cl_init_params params = {
        .profiling = true,
    };
    cl_init(cl, &params);

    bench_fill_init_size(test);

    test->buf = cl_create_buffer(cl, CL_MEM_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS, test->size);
    test->pipeline = cl_create_pipeline(cl, bench_fill_cs, "memset32");
}

static void
bench_fill_cleanup(struct bench_fill *test)
{
    struct cl *cl = &test->cl;

    cl_destroy_pipeline(cl, test->pipeline);
    cl_destroy_buffer(cl, test->buf);

    cl_cleanup(cl);
}

static void
bench_fill_dispatch(struct bench_fill *test)
{
    struct cl *cl = &test->cl;
    const size_t count = test->size / sizeof(cl_uint);
    const uint32_t loops = 5;

    cl_set_pipeline_arg(cl, test->pipeline, 0, &test->buf->mem, sizeof(test->buf->mem));
    cl_set_pipeline_arg(cl, test->pipeline, 1, &test->val, sizeof(test->val));

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
    struct bench_fill test = {
        .size = 0,
        .val = 0x12345677,
    };

    bench_fill_init(&test);
    bench_fill_dispatch(&test);
    bench_fill_cleanup(&test);

    return 0;
}
