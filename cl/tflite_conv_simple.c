/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "clutil.h"

static const char tflite_conv_simple_test_cs[] = {
#include "tflite_conv_simple_test.cl.inc"
};

struct tflite_conv_simple_test {
    cl_int width;
    cl_int height;
    cl_int slice_count;

    cl_int reduce_width;
    cl_int reduce_height;
    cl_int kernel_width;
    cl_int kernel_height;

    struct cl cl;

    struct cl_buffer *src_buf;
    struct cl_image *src_img;
    struct cl_buffer *dst_buf;
    struct cl_buffer *weight_buf;

    struct cl_pipeline *pipeline;
};

static void
tflite_conv_simple_test_init(struct tflite_conv_simple_test *test)
{
    struct cl *cl = &test->cl;

    const struct cl_init_params params = {
        .profiling = true,
    };
    cl_init(cl, &params);
    cl_log("device: %s", cl->dev->name);

    const size_t src_count = test->width * test->height * test->slice_count;
    const size_t src_size = sizeof(cl_half4) * src_count;
    test->src_buf = cl_create_buffer(cl, CL_MEM_READ_WRITE, src_size, NULL);
    test->src_img =
        cl_create_image(cl, CL_MEM_READ_WRITE, CL_RGBA, CL_HALF_FLOAT,
                        CL_MEM_OBJECT_IMAGE1D_BUFFER, src_count, 0, test->src_buf->mem, NULL);

    const size_t dst_count =
        (test->width / test->reduce_width) * (test->height / test->reduce_height);
    const size_t dst_size = sizeof(cl_half4) * dst_count;
    test->dst_buf = cl_create_buffer(cl, CL_MEM_READ_WRITE, dst_size, NULL);

    const size_t weight_count = test->kernel_width * test->kernel_height * test->slice_count;
    const size_t weight_size = sizeof(cl_half4) * weight_count;
    test->weight_buf = cl_create_buffer(cl, CL_MEM_READ_WRITE, weight_size, NULL);

    test->pipeline = cl_create_pipeline(cl, tflite_conv_simple_test_cs, "convert");
}

static void
tflite_conv_simple_test_cleanup(struct tflite_conv_simple_test *test)
{
    struct cl *cl = &test->cl;

    cl_destroy_pipeline(cl, test->pipeline);

    cl_destroy_buffer(cl, test->weight_buf);

    cl_destroy_buffer(cl, test->dst_buf);
    cl_destroy_image(cl, test->src_img);
    cl_destroy_buffer(cl, test->src_buf);

    cl_cleanup(cl);
}

static void
tflite_conv_simple_test_dispatch(struct tflite_conv_simple_test *test)
{
    struct cl *cl = &test->cl;
    const uint32_t loops = 4;
    const uint32_t repeat = 5;

    cl_set_pipeline_arg(cl, test->pipeline, 0, &test->dst_buf->mem, sizeof(test->dst_buf->mem));
    cl_set_pipeline_arg(cl, test->pipeline, 1, &test->src_img->mem, sizeof(test->src_img->mem));
    cl_set_pipeline_arg(cl, test->pipeline, 2, &test->weight_buf->mem,
                        sizeof(test->weight_buf->mem));

    const cl_int4 args[] = {
        [0] = {
            .x = test->width,
            .y = test->width * test->height,
            .z = test->slice_count,
            .w = test->width / test->reduce_width,
        },
        [1] = {
            .x = test->kernel_width,
            .y = test->kernel_height,
        },
    };
    cl_set_pipeline_arg(cl, test->pipeline, 3, &args[0], sizeof(args[0]));
    cl_set_pipeline_arg(cl, test->pipeline, 4, &args[1], sizeof(args[1]));

    for (uint32_t i = 0; i < loops; i++) {
        cl_event ev;
        cl_enqueue_pipeline(cl, test->pipeline, test->width / test->reduce_width,
                            test->height / test->reduce_height, repeat, 8, 8, 1, &ev);
        cl_finish(cl);

        cl_ulong start_ns;
        cl_ulong end_ns;
        cl_get_event_profiling_info(cl, ev, CL_PROFILING_COMMAND_START, &start_ns,
                                    sizeof(start_ns));
        cl_get_event_profiling_info(cl, ev, CL_PROFILING_COMMAND_END, &end_ns, sizeof(end_ns));
        cl_destroy_event(cl, ev);

        const float dur_ms = (float)(end_ns - start_ns) / 1000000;
        cl_log("iter %d took %.3f ms", i, dur_ms);
    }
}

int
main(void)
{
    struct tflite_conv_simple_test test = {
        .width = 512,
        .height = 288,
        .slice_count = 6,

        .reduce_width = 4,
        .reduce_height = 4,
        .kernel_width = 4,
        .kernel_height = 4,
    };

    tflite_conv_simple_test_init(&test);
    tflite_conv_simple_test_dispatch(&test);
    tflite_conv_simple_test_cleanup(&test);

    return 0;
}
