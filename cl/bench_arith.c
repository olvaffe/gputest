/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "clutil.h"

#define BENCH_ARITH_CS_OP_COUNT (10000 * 2 * 2)
static const char bench_arith_cs[] = "                         \n\
kernel void arith(global REPLACE_REAL_TYPE *dst)               \n\
{                                                              \n\
    const size_t idx = get_global_id(0);                       \n\
    REPLACE_REAL_TYPE x = (REPLACE_REAL_TYPE)idx;              \n\
    REPLACE_REAL_TYPE y = (REPLACE_REAL_TYPE)idx;              \n\
    __attribute__((opencl_unroll_hint(100)))                   \n\
    for (int i = 0; i < 10000; i++) {                          \n\
        x = (x * y) + y;                                       \n\
        y = (y * x) + x;                                       \n\
    }                                                          \n\
    dst[idx] = y;                                              \n\
}";

struct bench_arith {
    const char *type_name;
    uint32_t type_size;
    uint32_t type_width;

    struct cl cl;

    uint32_t global_work_size;
    uint64_t target_ops;

    struct cl_buffer *buf;
    struct cl_pipeline *pipeline;
};

static void
bench_arith_init_type(struct bench_arith *test)
{
    const char *width;
    for (width = test->type_name; *width != '\0'; width++) {
        if (isdigit(*width))
            break;
    }

    const size_t len = width - test->type_name;
    char name[32];
    if (len < sizeof(name)) {
        memcpy(name, test->type_name, len);
        name[len] = '\0';

        if (!strcmp(name, "char"))
            test->type_size = sizeof(cl_char);
        else if (!strcmp(name, "short"))
            test->type_size = sizeof(cl_short);
        else if (!strcmp(name, "int"))
            test->type_size = sizeof(cl_int);
        else if (!strcmp(name, "long"))
            test->type_size = sizeof(cl_long);
        else if (!strcmp(name, "half"))
            test->type_size = sizeof(cl_half);
        else if (!strcmp(name, "float"))
            test->type_size = sizeof(cl_float);
        else if (!strcmp(name, "double"))
            test->type_size = sizeof(cl_double);
    }

    test->type_width = *width != '\0' ? atoi(width) : 1;

    if (!test->type_size || !test->type_width || test->type_width > 16 ||
        (test->type_width & (test->type_width - 1)))
        cl_die("unknown type: %s", test->type_name);
}

static void
bench_arith_init_global_work_size(struct bench_arith *test)
{
    const uint64_t giga_ops = 1000ull * 1000 * 1000;
    const uint64_t tera_ops = giga_ops * 1000;
    const uint64_t target_ops = tera_ops / 10;
    struct cl *cl = &test->cl;

    const uint64_t work_item_ops = BENCH_ARITH_CS_OP_COUNT * test->type_width;
    test->global_work_size = target_ops / work_item_ops;

    const uint32_t align =
        cl->dev->max_compute_units * (cl->dev->preferred_work_group_size_multiple
                                          ? cl->dev->preferred_work_group_size_multiple
                                          : cl->dev->max_work_group_size);
    const uint32_t rem = test->global_work_size % align;
    if (rem)
        test->global_work_size += align - rem;

    test->target_ops = (uint64_t)test->global_work_size * work_item_ops;

    const int target_giga_ops = (int)(test->target_ops / 1000 / 1000 / 1000);
    cl_log("targeting %d giga ops using type %s: global work size %d", target_giga_ops,
           test->type_name, test->global_work_size);
}

static void
bench_arith_init_buffer(struct bench_arith *test)
{
    struct cl *cl = &test->cl;

    test->buf = cl_create_buffer(cl, CL_MEM_WRITE_ONLY,
                                 test->global_work_size * test->type_size * test->type_width);
}

static void
bench_arith_init_pipeline(struct bench_arith *test)
{
    const char keyword[] = "REPLACE_REAL_TYPE";
    const size_t keyword_len = sizeof(keyword) - 1;
    struct cl *cl = &test->cl;

    const size_t type_len = strlen(test->type_name);
    if (type_len > keyword_len)
        cl_die("type name too long");

    char *code = strdup(bench_arith_cs);
    char *p = code;
    while (true) {
        p = strstr(p, keyword);
        if (!p)
            break;

        memcpy(p, test->type_name, type_len);
        memset(p + type_len, ' ', keyword_len - type_len);
    }

    test->pipeline = cl_create_pipeline(cl, code, "arith");

    free(code);
}

static void
bench_arith_init(struct bench_arith *test)
{
    struct cl *cl = &test->cl;

    bench_arith_init_type(test);

    const struct cl_init_params params = {
        .profiling = true,
    };
    cl_init(cl, &params);
    cl_log("device: %s", cl->dev->name);

    bench_arith_init_global_work_size(test);
    bench_arith_init_buffer(test);
    bench_arith_init_pipeline(test);
}

static void
bench_arith_cleanup(struct bench_arith *test)
{
    struct cl *cl = &test->cl;

    cl_destroy_pipeline(cl, test->pipeline);
    cl_destroy_buffer(cl, test->buf);

    cl_cleanup(cl);
}

static void
bench_arith_dispatch(struct bench_arith *test)
{
    struct cl *cl = &test->cl;
    const uint32_t loops = 4;

    cl_set_pipeline_arg(cl, test->pipeline, 0, &test->buf->mem, sizeof(test->buf->mem));

    for (uint32_t i = 0; i < loops; i++) {
        cl_event ev;

        cl_enqueue_pipeline(cl, test->pipeline, test->global_work_size, 0, 0, &ev);
        cl_wait_event(cl, ev);

        cl_ulong start_ns;
        cl_ulong end_ns;
        cl_get_event_profiling_info(cl, ev, CL_PROFILING_COMMAND_START, &start_ns,
                                    sizeof(start_ns));
        cl_get_event_profiling_info(cl, ev, CL_PROFILING_COMMAND_END, &end_ns, sizeof(end_ns));
        cl_destroy_event(cl, ev);

        const uint32_t dur_us = (end_ns - start_ns) / 1000;
        const float gops = (float)test->target_ops / (end_ns - start_ns);
        cl_log("iter %d took %.3f ms: %.1f GOPS", i, (float)dur_us / 1000.0f, gops);
    }
}

int
main(int argc, char **argv)
{
    struct bench_arith test = {
        .type_name = NULL,
    };

    if (argc != 2)
        cl_die("usage: %s {char|short|int|long|half|float|double}[<N>]", argv[0]);
    test.type_name = argv[1];

    bench_arith_init(&test);
    bench_arith_dispatch(&test);
    bench_arith_cleanup(&test);

    return 0;
}
