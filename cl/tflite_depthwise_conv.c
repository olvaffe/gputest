/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "clutil.h"

static const char tflite_depthwise_conv_test_cs[] = "                      \n\
/* DepthwiseConv::GenerateCode */                                          \n\
#pragma OPENCL EXTENSION cl_khr_fp16 : enable                              \n\
kernel void                                                                \n\
main_function(global half4 *biases_buffer,                                 \n\
              global half4 *dst_tensor_buffer,                             \n\
              global half4 *weights_buffer,                                \n\
              read_only image1d_buffer_t src_tensor_image_buffer,          \n\
              int4 shared_int4_0,                                          \n\
              int4 shared_int4_1,                                          \n\
              int4 shared_int4_2,                                          \n\
              int4 shared_int4_3)                                          \n\
{                                                                          \n\
    const int dilation_x = shared_int4_0.x;                                \n\
    const int dilation_y = shared_int4_0.y;                                \n\
    const int dst_height = shared_int4_0.z;                                \n\
    const int dst_slices = shared_int4_0.w;                                \n\
    const int dst_width = shared_int4_1.x;                                 \n\
    const int kernel_size_x = shared_int4_1.y;                             \n\
    const int kernel_size_y = shared_int4_1.z;                             \n\
    const int kernels_total_size = shared_int4_1.w;                        \n\
    const int padding_x = shared_int4_2.x;                                 \n\
    const int padding_y = shared_int4_2.y;                                 \n\
    const int src_height = shared_int4_2.z;                                \n\
    const int src_width = shared_int4_2.w;                                 \n\
    const int stride_x = shared_int4_3.x;                                  \n\
    const int stride_y = shared_int4_3.y;                                  \n\
                                                                           \n\
    int X = get_global_id(0);                                              \n\
    int Y = get_global_id(1);                                              \n\
    int S = get_global_id(2);                                              \n\
    int x_src = X * stride_x + padding_x;                                  \n\
    int y_src = Y * stride_y + padding_y;                                  \n\
    if (X >= dst_width || Y >= dst_height ||                               \n\
        S >= dst_slices) {                                                 \n\
        return;                                                            \n\
    }                                                                      \n\
    half4 r = (half4)(0.0f);                                               \n\
    int fx_c = S * kernels_total_size;                                     \n\
    for (int ky = 0; ky < kernel_size_y; ++ky) {                           \n\
        int y_c = y_src + ky * dilation_y;                                 \n\
        bool inside_y = y_c >= 0 && y_c < src_height;                      \n\
        y_c = clamp(y_c, 0, src_height - 1);                               \n\
        for (int kx = 0; kx < kernel_size_x; ++kx) {                       \n\
            int x_c = x_src + kx * dilation_x;                             \n\
            bool inside_x = x_c >= 0 && x_c < src_width;                   \n\
            x_c = clamp(x_c, 0, src_width - 1);                            \n\
            half4 f = weights_buffer[fx_c];                                \n\
            half4 src_final;                                               \n\
            src_final = read_imageh(                                       \n\
                src_tensor_image_buffer,                                   \n\
                (((S)*src_height + (y_c)) * src_width + (x_c)));           \n\
            src_final = src_final * (half)(inside_y && inside_x);          \n\
            r += convert_half4(src_final * f);                             \n\
            fx_c++;                                                        \n\
        }                                                                  \n\
    }                                                                      \n\
    half4 res0 = convert_half4(r) + biases_buffer[(S)];                    \n\
    dst_tensor_buffer[(((S)*dst_height + (Y)) * dst_width +                \n\
                       (X))] = res0;                                       \n\
}";

struct tflite_depthwise_conv_test {
    cl_int src_width;
    cl_int src_height;
    cl_int dst_width;
    cl_int dst_height;
    cl_int slice_count;

    cl_int kernel_width;
    cl_int kernel_height;
    cl_int padding_x;
    cl_int padding_y;
    cl_int stride_x;
    cl_int stride_y;
    cl_int dilation_x;
    cl_int dilation_y;

    size_t buf_size;
    size_t src_offset;
    size_t src_size;
    size_t dst_offset;
    size_t dst_size;

    struct cl cl;

    struct cl_buffer *buf;
    struct cl_buffer *src_buf;
    struct cl_image *src_img;
    struct cl_buffer *dst_buf;

    struct cl_buffer *bias_buf;
    struct cl_buffer *weight_buf;

    struct cl_pipeline *pipeline;
};

static void
tflite_depthwise_conv_test_init(struct tflite_depthwise_conv_test *test)
{
    struct cl *cl = &test->cl;

    const struct cl_init_params params = {
        .profiling = true,
    };
    cl_init(cl, &params);
    cl_log("device: %s", cl->dev->name);

    test->buf = cl_create_buffer(cl, CL_MEM_READ_WRITE, test->buf_size, NULL);

    const size_t src_count = test->src_width * test->src_height * test->slice_count;
    if (test->src_size != sizeof(cl_half4) * src_count)
        cl_die("bad src size");

    test->src_buf =
        cl_suballoc_buffer(cl, test->buf, CL_MEM_READ_WRITE, test->src_offset, test->src_size);
    test->src_img =
        cl_create_image(cl, CL_MEM_READ_WRITE, CL_RGBA, CL_HALF_FLOAT,
                        CL_MEM_OBJECT_IMAGE1D_BUFFER, src_count, 0, test->src_buf->mem, NULL);

    const size_t dst_count = test->dst_width * test->dst_height * test->slice_count;
    if (test->dst_size != sizeof(cl_half4) * dst_count)
        cl_die("bad dst size");

    test->dst_buf =
        cl_suballoc_buffer(cl, test->buf, CL_MEM_READ_WRITE, test->dst_offset, test->dst_size);

    const size_t bias_size = sizeof(cl_half4) * test->slice_count;
    cl_half4 *biases = calloc(1, bias_size);
    test->bias_buf =
        cl_create_buffer(cl, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, bias_size, biases);
    free(biases);

    const size_t weight_size =
        sizeof(cl_half4) * test->slice_count * test->kernel_width * test->kernel_height;
    cl_half4 *weights = calloc(1, weight_size);
    test->weight_buf =
        cl_create_buffer(cl, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, weight_size, weights);
    free(weights);

    test->pipeline = cl_create_pipeline(cl, tflite_depthwise_conv_test_cs, "main_function");
}

static void
tflite_depthwise_conv_test_cleanup(struct tflite_depthwise_conv_test *test)
{
    struct cl *cl = &test->cl;

    cl_destroy_pipeline(cl, test->pipeline);

    cl_destroy_buffer(cl, test->bias_buf);
    cl_destroy_buffer(cl, test->weight_buf);

    cl_destroy_buffer(cl, test->dst_buf);
    cl_destroy_image(cl, test->src_img);
    cl_destroy_buffer(cl, test->src_buf);
    cl_destroy_buffer(cl, test->buf);

    cl_cleanup(cl);
}

static void
tflite_depthwise_conv_test_dispatch(struct tflite_depthwise_conv_test *test)
{
    struct cl *cl = &test->cl;
    const uint32_t loops = 4;

    cl_set_pipeline_arg(cl, test->pipeline, 0, &test->bias_buf->mem, sizeof(test->bias_buf->mem));
    cl_set_pipeline_arg(cl, test->pipeline, 1, &test->dst_buf->mem, sizeof(test->dst_buf->mem));
    cl_set_pipeline_arg(cl, test->pipeline, 2, &test->weight_buf->mem,
                        sizeof(test->weight_buf->mem));
    cl_set_pipeline_arg(cl, test->pipeline, 3, &test->src_img->mem, sizeof(test->src_img->mem));

    const cl_int4 shared[] = {
        [0] = {
            .x = test->dilation_x,
            .y = test->dilation_y,
            .z = test->dst_height,
            .w = test->slice_count,
        },
        [1] = {
            .x = test->dst_width,
            .y = test->kernel_width,
            .z = test->kernel_height,
            .w = test->kernel_width * test->kernel_height,
        },
        [2] = {
            .x = test->padding_x,
            .y = test->padding_y,
            .z = test->src_height,
            .w = test->src_width,
        },
        [3] = {
            .x = test->stride_x,
            .y = test->stride_y,
        },
    };
    cl_set_pipeline_arg(cl, test->pipeline, 4, &shared[0], sizeof(shared[0]));
    cl_set_pipeline_arg(cl, test->pipeline, 5, &shared[1], sizeof(shared[1]));
    cl_set_pipeline_arg(cl, test->pipeline, 6, &shared[2], sizeof(shared[2]));
    cl_set_pipeline_arg(cl, test->pipeline, 7, &shared[3], sizeof(shared[3]));

    for (uint32_t i = 0; i < loops; i++) {
        cl_event ev;

        cl_enqueue_pipeline(cl, test->pipeline, test->dst_width, test->dst_height,
                            test->slice_count, 128, 1, 2, &ev);
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
    struct tflite_depthwise_conv_test test = {
        .src_width = 512,
        .src_height = 288,
        .dst_width = 256,
        .dst_height = 144,
        .slice_count = 2,

        .kernel_width = 4,
        .kernel_height = 4,
        .padding_x = -1,
        .padding_y = -1,
        .stride_x = 2,
        .stride_y = 2,
        .dilation_x = 1,
        .dilation_y = 1,

        .buf_size = 14155776,
        .src_offset = 11796480,
        .src_size = 2359296,
        .dst_offset = 7077888,
        .dst_size = 589824,
    };

    tflite_depthwise_conv_test_init(&test);
    tflite_depthwise_conv_test_dispatch(&test);
    tflite_depthwise_conv_test_cleanup(&test);

    return 0;
}
