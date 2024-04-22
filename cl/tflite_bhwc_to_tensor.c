/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "clutil.h"

static const char tflite_bhwc_to_tensor_test_cs[] = "                  \n\
/* tflite CreateBhwcBufferToTensorOp */                                \n\
#pragma OPENCL EXTENSION cl_khr_fp16 : enable                          \n\
kernel void                                                            \n\
bhwc_to_tensor(global float *bhwc,                                     \n\
               global half4 *tensor,                                   \n\
               int4 shared_int4_0,                                     \n\
               int4 shared_int4_1)                                     \n\
{                                                                      \n\
    const int batch = shared_int4_0.x;                                 \n\
    const int channels = shared_int4_0.y;                              \n\
    const int height = shared_int4_0.z;                                \n\
    const int slices = shared_int4_0.w;                                \n\
    const int width = shared_int4_1.x;                                 \n\
    int linear_id = get_global_id(0);                                  \n\
    int x = linear_id / batch;                                         \n\
    int b = linear_id % batch;                                         \n\
    int y = get_global_id(1);                                          \n\
    int d = get_global_id(2);                                          \n\
                                                                       \n\
    if (x >= width || y >= height || d >= slices)                      \n\
        return;                                                        \n\
    int c = d * 4;                                                     \n\
    int index = ((b * height + y) * width + x) * channels + c;         \n\
    half4 result;                                                      \n\
    result.x = bhwc[index];                                            \n\
    result.y = c + 1 < channels ? bhwc[index + 1] : 1;                 \n\
    result.z = c + 2 < channels ? bhwc[index + 2] : 2;                 \n\
    result.w = c + 3 < channels ? bhwc[index + 3] : 3;                 \n\
    tensor[((((d)*height + y) * width + (x)) * batch + (b))] = result; \n\
}";

struct tflite_bhwc_to_tensor_test {
    cl_int width;
    cl_int height;
    cl_int channels;
    cl_int slices;
    cl_int batches;

    struct cl cl;
    struct cl_buffer *src;
    struct cl_buffer *dst;
    struct cl_pipeline *pipeline;

    void *random_input;
    size_t random_input_size;
};

static void
tflite_bhwc_to_tensor_test_init(struct tflite_bhwc_to_tensor_test *test)
{
    struct cl *cl = &test->cl;

    const struct cl_init_params params = {
        .profiling = true,
    };
    cl_init(cl, &params);
    cl_log("device: %s", cl->dev->name);

    const size_t item_count = test->width * test->height * test->slices * test->batches;
    const size_t src_size = item_count * 4 * sizeof(cl_float);
    const size_t dst_size = item_count * 4 * sizeof(cl_half);
    test->src = cl_create_buffer(cl, CL_MEM_READ_WRITE, src_size, NULL);
    test->dst = cl_create_buffer(cl, CL_MEM_READ_WRITE, dst_size, NULL);
    test->pipeline = cl_create_pipeline(cl, tflite_bhwc_to_tensor_test_cs, "bhwc_to_tensor");

    test->random_input_size = item_count * test->channels * sizeof(cl_float);
    test->random_input = malloc(test->random_input_size);
    for (uint32_t i = 0; i < item_count * test->channels; i++)
        ((cl_float *)test->random_input)[i] = (cl_float)i;
}

static void
tflite_bhwc_to_tensor_test_cleanup(struct tflite_bhwc_to_tensor_test *test)
{
    struct cl *cl = &test->cl;

    free(test->random_input);

    cl_destroy_pipeline(cl, test->pipeline);
    cl_destroy_buffer(cl, test->dst);
    cl_destroy_buffer(cl, test->src);
    cl_cleanup(cl);
}

static void
tflite_bhwc_to_tensor_test_dispatch(struct tflite_bhwc_to_tensor_test *test)
{
    struct cl *cl = &test->cl;
    const uint32_t loops = 4;

    cl_write_buffer(cl, test->src, test->random_input, test->random_input_size);

    cl_set_pipeline_arg(cl, test->pipeline, 0, &test->src->mem, sizeof(test->src->mem));
    cl_set_pipeline_arg(cl, test->pipeline, 1, &test->dst->mem, sizeof(test->dst->mem));
    const cl_int4 shared[] = {
        [0] = {
            .x = test->batches,
            .y = test->channels,
            .z = test->height,
            .w = test->slices,
        },
        [1] = {
            .x = test->width,
        },
    };
    cl_set_pipeline_arg(cl, test->pipeline, 2, &shared[0], sizeof(shared[0]));
    cl_set_pipeline_arg(cl, test->pipeline, 3, &shared[1], sizeof(shared[1]));

    for (uint32_t i = 0; i < loops; i++) {
        cl_event ev;

        cl_enqueue_pipeline(cl, test->pipeline, test->width, test->height, 0, 256, 1, 1, &ev);
        cl_wait_event(cl, ev);

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
    struct tflite_bhwc_to_tensor_test test = {
        .width = 512,
        .height = 288,
        .channels = 3,
        .slices = 1,
        .batches = 1,
    };

    tflite_bhwc_to_tensor_test_init(&test);
    tflite_bhwc_to_tensor_test_dispatch(&test);
    tflite_bhwc_to_tensor_test_cleanup(&test);

    return 0;
}
