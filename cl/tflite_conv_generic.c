/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "clutil.h"

static const char tflite_conv_generic_test_cs[] = "                               \n\
/* ConvGeneric::GenerateConv */                                                   \n\
#pragma OPENCL EXTENSION cl_khr_fp16 : enable                                     \n\
kernel void                                                                       \n\
main_function(constant half4 *biases_buffer,                                      \n\
              global half4 *dst_tensor_buffer,                                    \n\
              constant half4 *weights_buffer,                                     \n\
              read_only image1d_buffer_t src_tensor_image_buffer,                 \n\
              int4 shared_int4_0,                                                 \n\
              int4 shared_int4_1,                                                 \n\
              int4 shared_int4_2,                                                 \n\
              int4 shared_int4_3,                                                 \n\
              half4 shared_half4_0)                                               \n\
{                                                                                 \n\
    const int dilation_x = shared_int4_0.x;                                       \n\
    const int dilation_y = shared_int4_0.y;                                       \n\
    const int dst_height = shared_int4_0.z;                                       \n\
    const int dst_slices = shared_int4_0.w;                                       \n\
    const int dst_width = shared_int4_1.x;                                        \n\
    const int kernel_size_x = shared_int4_1.y;                                    \n\
    const int kernel_size_y = shared_int4_1.z;                                    \n\
    const int padding_x = shared_int4_1.w;                                        \n\
    const int padding_y = shared_int4_2.x;                                        \n\
    const int src_height = shared_int4_2.y;                                       \n\
    const int src_slice_stride = shared_int4_2.z;                                 \n\
    const int src_slices = shared_int4_2.w;                                       \n\
    const int src_width = shared_int4_3.x;                                        \n\
    const int stride_x = shared_int4_3.y;                                         \n\
    const int stride_y = shared_int4_3.z;                                         \n\
    const half activation_min = shared_half4_0.x;                                 \n\
                                                                                  \n\
    int DST_X = get_global_id(0);                                                 \n\
    int DST_Y = get_global_id(1);                                                 \n\
    int DST_S = get_global_id(2);                                                 \n\
    DST_X *= 4;                                                                   \n\
    DST_Y *= 2;                                                                   \n\
    DST_S *= 2;                                                                   \n\
    if (DST_S >= dst_slices)                                                      \n\
        return;                                                                   \n\
    if (DST_X >= dst_width || DST_Y >= dst_height || DST_S >= dst_slices) {       \n\
        return;                                                                   \n\
    }                                                                             \n\
    half4 r_w0_h0_s0 = (half4)(0.0f);                                             \n\
    half4 r_w1_h0_s0 = (half4)(0.0f);                                             \n\
    half4 r_w2_h0_s0 = (half4)(0.0f);                                             \n\
    half4 r_w3_h0_s0 = (half4)(0.0f);                                             \n\
    half4 r_w0_h1_s0 = (half4)(0.0f);                                             \n\
    half4 r_w1_h1_s0 = (half4)(0.0f);                                             \n\
    half4 r_w2_h1_s0 = (half4)(0.0f);                                             \n\
    half4 r_w3_h1_s0 = (half4)(0.0f);                                             \n\
    half4 r_w0_h0_s1 = (half4)(0.0f);                                             \n\
    half4 r_w1_h0_s1 = (half4)(0.0f);                                             \n\
    half4 r_w2_h0_s1 = (half4)(0.0f);                                             \n\
    half4 r_w3_h0_s1 = (half4)(0.0f);                                             \n\
    half4 r_w0_h1_s1 = (half4)(0.0f);                                             \n\
    half4 r_w1_h1_s1 = (half4)(0.0f);                                             \n\
    half4 r_w2_h1_s1 = (half4)(0.0f);                                             \n\
    half4 r_w3_h1_s1 = (half4)(0.0f);                                             \n\
    int xc0 = (DST_X + 0) * stride_x + padding_x;                                 \n\
    int xc1 = (DST_X + 1) * stride_x + padding_x;                                 \n\
    int xc2 = (DST_X + 2) * stride_x + padding_x;                                 \n\
    int xc3 = (DST_X + 3) * stride_x + padding_x;                                 \n\
    int yc0 = (DST_Y + 0) * stride_y + padding_y;                                 \n\
    int yc1 = (DST_Y + 1) * stride_y + padding_y;                                 \n\
    __constant half4 *weights_cache;                                              \n\
    __constant half4 *filters_loc = weights_buffer + DST_S * 4 * src_slices *     \n\
                                                         kernel_size_x *          \n\
                                                         kernel_size_y;           \n\
    for (int ky = 0; ky < kernel_size_y; ++ky) {                                  \n\
        int yck0 = ky * dilation_y + yc0;                                         \n\
        bool in_y0 = yck0 >= 0 && yck0 < src_height;                              \n\
        int yck1 = ky * dilation_y + yc1;                                         \n\
        bool in_y1 = yck1 >= 0 && yck1 < src_height;                              \n\
        for (int kx = 0; kx < kernel_size_x; ++kx) {                              \n\
            int xck0 = kx * dilation_x + xc0;                                     \n\
            bool in_x0 = xck0 >= 0 && xck0 < src_width;                           \n\
            int xck1 = kx * dilation_x + xc1;                                     \n\
            bool in_x1 = xck1 >= 0 && xck1 < src_width;                           \n\
            int xck2 = kx * dilation_x + xc2;                                     \n\
            bool in_x2 = xck2 >= 0 && xck2 < src_width;                           \n\
            int xck3 = kx * dilation_x + xc3;                                     \n\
            bool in_x3 = xck3 >= 0 && xck3 < src_width;                           \n\
            int addr_w0_h0 =                                                      \n\
                (((0) * src_height + (yck0)) * src_width + (xck0));               \n\
            addr_w0_h0 = select(-1, addr_w0_h0, (in_x0 && in_y0));                \n\
            int ds_w0_h0 = select(0, src_slice_stride, (in_x0 && in_y0));         \n\
            int addr_w1_h0 =                                                      \n\
                (((0) * src_height + (yck0)) * src_width + (xck1));               \n\
            addr_w1_h0 = select(-1, addr_w1_h0, (in_x1 && in_y0));                \n\
            int ds_w1_h0 = select(0, src_slice_stride, (in_x1 && in_y0));         \n\
            int addr_w2_h0 =                                                      \n\
                (((0) * src_height + (yck0)) * src_width + (xck2));               \n\
            addr_w2_h0 = select(-1, addr_w2_h0, (in_x2 && in_y0));                \n\
            int ds_w2_h0 = select(0, src_slice_stride, (in_x2 && in_y0));         \n\
            int addr_w3_h0 =                                                      \n\
                (((0) * src_height + (yck0)) * src_width + (xck3));               \n\
            addr_w3_h0 = select(-1, addr_w3_h0, (in_x3 && in_y0));                \n\
            int ds_w3_h0 = select(0, src_slice_stride, (in_x3 && in_y0));         \n\
            int addr_w0_h1 =                                                      \n\
                (((0) * src_height + (yck1)) * src_width + (xck0));               \n\
            addr_w0_h1 = select(-1, addr_w0_h1, (in_x0 && in_y1));                \n\
            int ds_w0_h1 = select(0, src_slice_stride, (in_x0 && in_y1));         \n\
            int addr_w1_h1 =                                                      \n\
                (((0) * src_height + (yck1)) * src_width + (xck1));               \n\
            addr_w1_h1 = select(-1, addr_w1_h1, (in_x1 && in_y1));                \n\
            int ds_w1_h1 = select(0, src_slice_stride, (in_x1 && in_y1));         \n\
            int addr_w2_h1 =                                                      \n\
                (((0) * src_height + (yck1)) * src_width + (xck2));               \n\
            addr_w2_h1 = select(-1, addr_w2_h1, (in_x2 && in_y1));                \n\
            int ds_w2_h1 = select(0, src_slice_stride, (in_x2 && in_y1));         \n\
            int addr_w3_h1 =                                                      \n\
                (((0) * src_height + (yck1)) * src_width + (xck3));               \n\
            addr_w3_h1 = select(-1, addr_w3_h1, (in_x3 && in_y1));                \n\
            int ds_w3_h1 = select(0, src_slice_stride, (in_x3 && in_y1));         \n\
            int s = 0;                                                            \n\
            do {                                                                  \n\
                half4 src_w0_h0;                                                  \n\
                half4 src_w1_h0;                                                  \n\
                half4 src_w2_h0;                                                  \n\
                half4 src_w3_h0;                                                  \n\
                half4 src_w0_h1;                                                  \n\
                half4 src_w1_h1;                                                  \n\
                half4 src_w2_h1;                                                  \n\
                half4 src_w3_h1;                                                  \n\
                weights_cache = filters_loc;                                      \n\
                src_w0_h0 = read_imageh(src_tensor_image_buffer, addr_w0_h0);     \n\
                addr_w0_h0 += ds_w0_h0;                                           \n\
                src_w1_h0 = read_imageh(src_tensor_image_buffer, addr_w1_h0);     \n\
                addr_w1_h0 += ds_w1_h0;                                           \n\
                src_w2_h0 = read_imageh(src_tensor_image_buffer, addr_w2_h0);     \n\
                addr_w2_h0 += ds_w2_h0;                                           \n\
                src_w3_h0 = read_imageh(src_tensor_image_buffer, addr_w3_h0);     \n\
                addr_w3_h0 += ds_w3_h0;                                           \n\
                src_w0_h1 = read_imageh(src_tensor_image_buffer, addr_w0_h1);     \n\
                addr_w0_h1 += ds_w0_h1;                                           \n\
                src_w1_h1 = read_imageh(src_tensor_image_buffer, addr_w1_h1);     \n\
                addr_w1_h1 += ds_w1_h1;                                           \n\
                src_w2_h1 = read_imageh(src_tensor_image_buffer, addr_w2_h1);     \n\
                addr_w2_h1 += ds_w2_h1;                                           \n\
                src_w3_h1 = read_imageh(src_tensor_image_buffer, addr_w3_h1);     \n\
                addr_w3_h1 += ds_w3_h1;                                           \n\
                s += 1;                                                           \n\
                r_w0_h0_s0 = fma(weights_cache[0], src_w0_h0.x, r_w0_h0_s0);      \n\
                r_w1_h0_s0 = fma(weights_cache[0], src_w1_h0.x, r_w1_h0_s0);      \n\
                r_w2_h0_s0 = fma(weights_cache[0], src_w2_h0.x, r_w2_h0_s0);      \n\
                r_w3_h0_s0 = fma(weights_cache[0], src_w3_h0.x, r_w3_h0_s0);      \n\
                r_w0_h1_s0 = fma(weights_cache[0], src_w0_h1.x, r_w0_h1_s0);      \n\
                r_w1_h1_s0 = fma(weights_cache[0], src_w1_h1.x, r_w1_h1_s0);      \n\
                r_w2_h1_s0 = fma(weights_cache[0], src_w2_h1.x, r_w2_h1_s0);      \n\
                r_w3_h1_s0 = fma(weights_cache[0], src_w3_h1.x, r_w3_h1_s0);      \n\
                r_w0_h0_s0 = fma(weights_cache[1], src_w0_h0.y, r_w0_h0_s0);      \n\
                r_w1_h0_s0 = fma(weights_cache[1], src_w1_h0.y, r_w1_h0_s0);      \n\
                r_w2_h0_s0 = fma(weights_cache[1], src_w2_h0.y, r_w2_h0_s0);      \n\
                r_w3_h0_s0 = fma(weights_cache[1], src_w3_h0.y, r_w3_h0_s0);      \n\
                r_w0_h1_s0 = fma(weights_cache[1], src_w0_h1.y, r_w0_h1_s0);      \n\
                r_w1_h1_s0 = fma(weights_cache[1], src_w1_h1.y, r_w1_h1_s0);      \n\
                r_w2_h1_s0 = fma(weights_cache[1], src_w2_h1.y, r_w2_h1_s0);      \n\
                r_w3_h1_s0 = fma(weights_cache[1], src_w3_h1.y, r_w3_h1_s0);      \n\
                r_w0_h0_s0 = fma(weights_cache[2], src_w0_h0.z, r_w0_h0_s0);      \n\
                r_w1_h0_s0 = fma(weights_cache[2], src_w1_h0.z, r_w1_h0_s0);      \n\
                r_w2_h0_s0 = fma(weights_cache[2], src_w2_h0.z, r_w2_h0_s0);      \n\
                r_w3_h0_s0 = fma(weights_cache[2], src_w3_h0.z, r_w3_h0_s0);      \n\
                r_w0_h1_s0 = fma(weights_cache[2], src_w0_h1.z, r_w0_h1_s0);      \n\
                r_w1_h1_s0 = fma(weights_cache[2], src_w1_h1.z, r_w1_h1_s0);      \n\
                r_w2_h1_s0 = fma(weights_cache[2], src_w2_h1.z, r_w2_h1_s0);      \n\
                r_w3_h1_s0 = fma(weights_cache[2], src_w3_h1.z, r_w3_h1_s0);      \n\
                r_w0_h0_s0 = fma(weights_cache[3], src_w0_h0.w, r_w0_h0_s0);      \n\
                r_w1_h0_s0 = fma(weights_cache[3], src_w1_h0.w, r_w1_h0_s0);      \n\
                r_w2_h0_s0 = fma(weights_cache[3], src_w2_h0.w, r_w2_h0_s0);      \n\
                r_w3_h0_s0 = fma(weights_cache[3], src_w3_h0.w, r_w3_h0_s0);      \n\
                r_w0_h1_s0 = fma(weights_cache[3], src_w0_h1.w, r_w0_h1_s0);      \n\
                r_w1_h1_s0 = fma(weights_cache[3], src_w1_h1.w, r_w1_h1_s0);      \n\
                r_w2_h1_s0 = fma(weights_cache[3], src_w2_h1.w, r_w2_h1_s0);      \n\
                r_w3_h1_s0 = fma(weights_cache[3], src_w3_h1.w, r_w3_h1_s0);      \n\
                r_w0_h0_s1 = fma(weights_cache[4], src_w0_h0.x, r_w0_h0_s1);      \n\
                r_w1_h0_s1 = fma(weights_cache[4], src_w1_h0.x, r_w1_h0_s1);      \n\
                r_w2_h0_s1 = fma(weights_cache[4], src_w2_h0.x, r_w2_h0_s1);      \n\
                r_w3_h0_s1 = fma(weights_cache[4], src_w3_h0.x, r_w3_h0_s1);      \n\
                r_w0_h1_s1 = fma(weights_cache[4], src_w0_h1.x, r_w0_h1_s1);      \n\
                r_w1_h1_s1 = fma(weights_cache[4], src_w1_h1.x, r_w1_h1_s1);      \n\
                r_w2_h1_s1 = fma(weights_cache[4], src_w2_h1.x, r_w2_h1_s1);      \n\
                r_w3_h1_s1 = fma(weights_cache[4], src_w3_h1.x, r_w3_h1_s1);      \n\
                r_w0_h0_s1 = fma(weights_cache[5], src_w0_h0.y, r_w0_h0_s1);      \n\
                r_w1_h0_s1 = fma(weights_cache[5], src_w1_h0.y, r_w1_h0_s1);      \n\
                r_w2_h0_s1 = fma(weights_cache[5], src_w2_h0.y, r_w2_h0_s1);      \n\
                r_w3_h0_s1 = fma(weights_cache[5], src_w3_h0.y, r_w3_h0_s1);      \n\
                r_w0_h1_s1 = fma(weights_cache[5], src_w0_h1.y, r_w0_h1_s1);      \n\
                r_w1_h1_s1 = fma(weights_cache[5], src_w1_h1.y, r_w1_h1_s1);      \n\
                r_w2_h1_s1 = fma(weights_cache[5], src_w2_h1.y, r_w2_h1_s1);      \n\
                r_w3_h1_s1 = fma(weights_cache[5], src_w3_h1.y, r_w3_h1_s1);      \n\
                r_w0_h0_s1 = fma(weights_cache[6], src_w0_h0.z, r_w0_h0_s1);      \n\
                r_w1_h0_s1 = fma(weights_cache[6], src_w1_h0.z, r_w1_h0_s1);      \n\
                r_w2_h0_s1 = fma(weights_cache[6], src_w2_h0.z, r_w2_h0_s1);      \n\
                r_w3_h0_s1 = fma(weights_cache[6], src_w3_h0.z, r_w3_h0_s1);      \n\
                r_w0_h1_s1 = fma(weights_cache[6], src_w0_h1.z, r_w0_h1_s1);      \n\
                r_w1_h1_s1 = fma(weights_cache[6], src_w1_h1.z, r_w1_h1_s1);      \n\
                r_w2_h1_s1 = fma(weights_cache[6], src_w2_h1.z, r_w2_h1_s1);      \n\
                r_w3_h1_s1 = fma(weights_cache[6], src_w3_h1.z, r_w3_h1_s1);      \n\
                r_w0_h0_s1 = fma(weights_cache[7], src_w0_h0.w, r_w0_h0_s1);      \n\
                r_w1_h0_s1 = fma(weights_cache[7], src_w1_h0.w, r_w1_h0_s1);      \n\
                r_w2_h0_s1 = fma(weights_cache[7], src_w2_h0.w, r_w2_h0_s1);      \n\
                r_w3_h0_s1 = fma(weights_cache[7], src_w3_h0.w, r_w3_h0_s1);      \n\
                r_w0_h1_s1 = fma(weights_cache[7], src_w0_h1.w, r_w0_h1_s1);      \n\
                r_w1_h1_s1 = fma(weights_cache[7], src_w1_h1.w, r_w1_h1_s1);      \n\
                r_w2_h1_s1 = fma(weights_cache[7], src_w2_h1.w, r_w2_h1_s1);      \n\
                r_w3_h1_s1 = fma(weights_cache[7], src_w3_h1.w, r_w3_h1_s1);      \n\
                filters_loc += 8;                                                 \n\
            } while (s < src_slices);                                             \n\
        };                                                                        \n\
    };                                                                            \n\
    weights_cache = biases_buffer + DST_S;                                        \n\
    if (DST_S + 0 >= dst_slices)                                                  \n\
        return;                                                                   \n\
    {                                                                             \n\
        half4 bias_val = convert_half4(weights_cache[0]);                         \n\
        {                                                                         \n\
            half4 res = convert_half4(r_w0_h0_s0) + bias_val;                     \n\
            {                                                                     \n\
                                                                                  \n\
                half4 res_final;                                                  \n\
                {                                                                 \n\
                    {                                                             \n\
                        res_final = max(res, (half4)(activation_min));            \n\
                    }                                                             \n\
                }                                                                 \n\
                dst_tensor_buffer[(((DST_S + 0) * dst_height + (DST_Y + 0)) *     \n\
                                       dst_width +                                \n\
                                   (DST_X + 0))] = res_final;                     \n\
            };                                                                    \n\
        }                                                                         \n\
        if (DST_X + 1 < dst_width) {                                              \n\
            half4 res = convert_half4(r_w1_h0_s0) + bias_val;                     \n\
            {                                                                     \n\
                                                                                  \n\
                half4 res_final;                                                  \n\
                {                                                                 \n\
                    {                                                             \n\
                        res_final = max(res, (half4)(activation_min));            \n\
                    }                                                             \n\
                }                                                                 \n\
                dst_tensor_buffer[(((DST_S + 0) * dst_height + (DST_Y + 0)) *     \n\
                                       dst_width +                                \n\
                                   (DST_X + 1))] = res_final;                     \n\
            };                                                                    \n\
        }                                                                         \n\
        if (DST_X + 2 < dst_width) {                                              \n\
            half4 res = convert_half4(r_w2_h0_s0) + bias_val;                     \n\
            {                                                                     \n\
                                                                                  \n\
                half4 res_final;                                                  \n\
                {                                                                 \n\
                    {                                                             \n\
                        res_final = max(res, (half4)(activation_min));            \n\
                    }                                                             \n\
                }                                                                 \n\
                dst_tensor_buffer[(((DST_S + 0) * dst_height + (DST_Y + 0)) *     \n\
                                       dst_width +                                \n\
                                   (DST_X + 2))] = res_final;                     \n\
            };                                                                    \n\
        }                                                                         \n\
        if (DST_X + 3 < dst_width) {                                              \n\
            half4 res = convert_half4(r_w3_h0_s0) + bias_val;                     \n\
            {                                                                     \n\
                                                                                  \n\
                half4 res_final;                                                  \n\
                {                                                                 \n\
                    {                                                             \n\
                        res_final = max(res, (half4)(activation_min));            \n\
                    }                                                             \n\
                }                                                                 \n\
                dst_tensor_buffer[(((DST_S + 0) * dst_height + (DST_Y + 0)) *     \n\
                                       dst_width +                                \n\
                                   (DST_X + 3))] = res_final;                     \n\
            };                                                                    \n\
        }                                                                         \n\
        if (DST_Y + 1 < dst_height) {                                             \n\
            half4 res = convert_half4(r_w0_h1_s0) + bias_val;                     \n\
            {                                                                     \n\
                                                                                  \n\
                half4 res_final;                                                  \n\
                {                                                                 \n\
                    {                                                             \n\
                        res_final = max(res, (half4)(activation_min));            \n\
                    }                                                             \n\
                }                                                                 \n\
                dst_tensor_buffer[(((DST_S + 0) * dst_height + (DST_Y + 1)) *     \n\
                                       dst_width +                                \n\
                                   (DST_X + 0))] = res_final;                     \n\
            };                                                                    \n\
        }                                                                         \n\
        if (DST_X + 1 < dst_width && DST_Y + 1 < dst_height) {                    \n\
            half4 res = convert_half4(r_w1_h1_s0) + bias_val;                     \n\
            {                                                                     \n\
                                                                                  \n\
                half4 res_final;                                                  \n\
                {                                                                 \n\
                    {                                                             \n\
                        res_final = max(res, (half4)(activation_min));            \n\
                    }                                                             \n\
                }                                                                 \n\
                dst_tensor_buffer[(((DST_S + 0) * dst_height + (DST_Y + 1)) *     \n\
                                       dst_width +                                \n\
                                   (DST_X + 1))] = res_final;                     \n\
            };                                                                    \n\
        }                                                                         \n\
        if (DST_X + 2 < dst_width && DST_Y + 1 < dst_height) {                    \n\
            half4 res = convert_half4(r_w2_h1_s0) + bias_val;                     \n\
            {                                                                     \n\
                                                                                  \n\
                half4 res_final;                                                  \n\
                {                                                                 \n\
                    {                                                             \n\
                        res_final = max(res, (half4)(activation_min));            \n\
                    }                                                             \n\
                }                                                                 \n\
                dst_tensor_buffer[(((DST_S + 0) * dst_height + (DST_Y + 1)) *     \n\
                                       dst_width +                                \n\
                                   (DST_X + 2))] = res_final;                     \n\
            };                                                                    \n\
        }                                                                         \n\
        if (DST_X + 3 < dst_width && DST_Y + 1 < dst_height) {                    \n\
            half4 res = convert_half4(r_w3_h1_s0) + bias_val;                     \n\
            {                                                                     \n\
                                                                                  \n\
                half4 res_final;                                                  \n\
                {                                                                 \n\
                    {                                                             \n\
                        res_final = max(res, (half4)(activation_min));            \n\
                    }                                                             \n\
                }                                                                 \n\
                dst_tensor_buffer[(((DST_S + 0) * dst_height + (DST_Y + 1)) *     \n\
                                       dst_width +                                \n\
                                   (DST_X + 3))] = res_final;                     \n\
            };                                                                    \n\
        }                                                                         \n\
    }                                                                             \n\
    if (DST_S + 1 >= dst_slices)                                                  \n\
        return;                                                                   \n\
    {                                                                             \n\
        half4 bias_val = convert_half4(weights_cache[1]);                         \n\
        {                                                                         \n\
            half4 res = convert_half4(r_w0_h0_s1) + bias_val;                     \n\
            {                                                                     \n\
                                                                                  \n\
                half4 res_final;                                                  \n\
                {                                                                 \n\
                    {                                                             \n\
                        res_final = max(res, (half4)(activation_min));            \n\
                    }                                                             \n\
                }                                                                 \n\
                dst_tensor_buffer[(((DST_S + 1) * dst_height + (DST_Y + 0)) *     \n\
                                       dst_width +                                \n\
                                   (DST_X + 0))] = res_final;                     \n\
            };                                                                    \n\
        }                                                                         \n\
        if (DST_X + 1 < dst_width) {                                              \n\
            half4 res = convert_half4(r_w1_h0_s1) + bias_val;                     \n\
            {                                                                     \n\
                                                                                  \n\
                half4 res_final;                                                  \n\
                {                                                                 \n\
                    {                                                             \n\
                        res_final = max(res, (half4)(activation_min));            \n\
                    }                                                             \n\
                }                                                                 \n\
                dst_tensor_buffer[(((DST_S + 1) * dst_height + (DST_Y + 0)) *     \n\
                                       dst_width +                                \n\
                                   (DST_X + 1))] = res_final;                     \n\
            };                                                                    \n\
        }                                                                         \n\
        if (DST_X + 2 < dst_width) {                                              \n\
            half4 res = convert_half4(r_w2_h0_s1) + bias_val;                     \n\
            {                                                                     \n\
                                                                                  \n\
                half4 res_final;                                                  \n\
                {                                                                 \n\
                    {                                                             \n\
                        res_final = max(res, (half4)(activation_min));            \n\
                    }                                                             \n\
                }                                                                 \n\
                dst_tensor_buffer[(((DST_S + 1) * dst_height + (DST_Y + 0)) *     \n\
                                       dst_width +                                \n\
                                   (DST_X + 2))] = res_final;                     \n\
            };                                                                    \n\
        }                                                                         \n\
        if (DST_X + 3 < dst_width) {                                              \n\
            half4 res = convert_half4(r_w3_h0_s1) + bias_val;                     \n\
            {                                                                     \n\
                                                                                  \n\
                half4 res_final;                                                  \n\
                {                                                                 \n\
                    {                                                             \n\
                        res_final = max(res, (half4)(activation_min));            \n\
                    }                                                             \n\
                }                                                                 \n\
                dst_tensor_buffer[(((DST_S + 1) * dst_height + (DST_Y + 0)) *     \n\
                                       dst_width +                                \n\
                                   (DST_X + 3))] = res_final;                     \n\
            };                                                                    \n\
        }                                                                         \n\
        if (DST_Y + 1 < dst_height) {                                             \n\
            half4 res = convert_half4(r_w0_h1_s1) + bias_val;                     \n\
            {                                                                     \n\
                                                                                  \n\
                half4 res_final;                                                  \n\
                {                                                                 \n\
                    {                                                             \n\
                        res_final = max(res, (half4)(activation_min));            \n\
                    }                                                             \n\
                }                                                                 \n\
                dst_tensor_buffer[(((DST_S + 1) * dst_height + (DST_Y + 1)) *     \n\
                                       dst_width +                                \n\
                                   (DST_X + 0))] = res_final;                     \n\
            };                                                                    \n\
        }                                                                         \n\
        if (DST_X + 1 < dst_width && DST_Y + 1 < dst_height) {                    \n\
            half4 res = convert_half4(r_w1_h1_s1) + bias_val;                     \n\
            {                                                                     \n\
                                                                                  \n\
                half4 res_final;                                                  \n\
                {                                                                 \n\
                    {                                                             \n\
                        res_final = max(res, (half4)(activation_min));            \n\
                    }                                                             \n\
                }                                                                 \n\
                dst_tensor_buffer[(((DST_S + 1) * dst_height + (DST_Y + 1)) *     \n\
                                       dst_width +                                \n\
                                   (DST_X + 1))] = res_final;                     \n\
            };                                                                    \n\
        }                                                                         \n\
        if (DST_X + 2 < dst_width && DST_Y + 1 < dst_height) {                    \n\
            half4 res = convert_half4(r_w2_h1_s1) + bias_val;                     \n\
            {                                                                     \n\
                                                                                  \n\
                half4 res_final;                                                  \n\
                {                                                                 \n\
                    {                                                             \n\
                        res_final = max(res, (half4)(activation_min));            \n\
                    }                                                             \n\
                }                                                                 \n\
                dst_tensor_buffer[(((DST_S + 1) * dst_height + (DST_Y + 1)) *     \n\
                                       dst_width +                                \n\
                                   (DST_X + 2))] = res_final;                     \n\
            };                                                                    \n\
        }                                                                         \n\
        if (DST_X + 3 < dst_width && DST_Y + 1 < dst_height) {                    \n\
            half4 res = convert_half4(r_w3_h1_s1) + bias_val;                     \n\
            {                                                                     \n\
                                                                                  \n\
                half4 res_final;                                                  \n\
                {                                                                 \n\
                    {                                                             \n\
                        res_final = max(res, (half4)(activation_min));            \n\
                    }                                                             \n\
                }                                                                 \n\
                dst_tensor_buffer[(((DST_S + 1) * dst_height + (DST_Y + 1)) *     \n\
                                       dst_width +                                \n\
                                   (DST_X + 3))] = res_final;                     \n\
            };                                                                    \n\
        }                                                                         \n\
    }                                                                             \n\
}";

struct tflite_conv_generic_test {
    cl_int src_width;
    cl_int src_height;
    cl_int src_slice_count;
    cl_int dst_width;
    cl_int dst_height;
    cl_int dst_slice_count;

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
tflite_conv_generic_test_init(struct tflite_conv_generic_test *test)
{
    struct cl *cl = &test->cl;

    const struct cl_init_params params = {
        .profiling = true,
    };
    cl_init(cl, &params);
    cl_log("device: %s", cl->dev->name);

    test->buf = cl_create_buffer(cl, CL_MEM_READ_WRITE, test->buf_size, NULL);

    const size_t src_count = test->src_width * test->src_height * test->src_slice_count;
    if (test->src_size != sizeof(cl_half4) * src_count)
        cl_die("bad src size");

    test->src_buf =
        cl_suballoc_buffer(cl, test->buf, CL_MEM_READ_WRITE, test->src_offset, test->src_size);
    test->src_img =
        cl_create_image(cl, CL_MEM_READ_WRITE, CL_RGBA, CL_HALF_FLOAT,
                        CL_MEM_OBJECT_IMAGE1D_BUFFER, src_count, 0, test->src_buf->mem, NULL);

    const size_t dst_count = test->dst_width * test->dst_height * test->dst_slice_count;
    if (test->dst_size != sizeof(cl_half4) * dst_count)
        cl_die("bad dst size");

    test->dst_buf =
        cl_suballoc_buffer(cl, test->buf, CL_MEM_READ_WRITE, test->dst_offset, test->dst_size);

    const size_t bias_size = sizeof(cl_half4) * test->dst_slice_count;
    cl_half4 *biases = calloc(1, bias_size);
    test->bias_buf =
        cl_create_buffer(cl, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, bias_size, biases);
    free(biases);

    const size_t weight_size =
        sizeof(cl_half4) * test->src_slice_count * 8 * test->kernel_width * test->kernel_height;
    cl_half4 *weights = calloc(1, weight_size);
    test->weight_buf =
        cl_create_buffer(cl, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, weight_size, weights);
    free(weights);

    test->pipeline = cl_create_pipeline(cl, tflite_conv_generic_test_cs, "main_function");
}

static void
tflite_conv_generic_test_cleanup(struct tflite_conv_generic_test *test)
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
tflite_conv_generic_test_dispatch(struct tflite_conv_generic_test *test)
{
    struct cl *cl = &test->cl;
    const uint32_t loops = 4;
    const uint32_t dispatches = 100;

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
            .w = test->dst_slice_count,
        },
        [1] = {
            .x = test->dst_width,
            .y = test->kernel_width,
            .z = test->kernel_height,
            .w = test->padding_x,
        },
        [2] = {
            .x = test->padding_y,
            .y = test->src_height,
            .z = test->src_width * test->src_height,
            .w = test->src_slice_count,
        },
        [3] = {
            .x = test->src_width,
            .y = test->stride_x,
            .z = test->stride_y,
        },
    };
    cl_set_pipeline_arg(cl, test->pipeline, 4, &shared[0], sizeof(shared[0]));
    cl_set_pipeline_arg(cl, test->pipeline, 5, &shared[1], sizeof(shared[1]));
    cl_set_pipeline_arg(cl, test->pipeline, 6, &shared[2], sizeof(shared[2]));
    cl_set_pipeline_arg(cl, test->pipeline, 7, &shared[3], sizeof(shared[3]));

    const cl_half4 what[] = { 0 };
    cl_set_pipeline_arg(cl, test->pipeline, 8, &what, sizeof(what));

    for (uint32_t i = 0; i < loops; i++) {
        cl_event start_ev;
        cl_event end_ev;

        for (uint32_t j = 0; j < dispatches; j++) {
            cl_event *ev = j == 0 ? &start_ev : j == dispatches - 1 ? &end_ev : NULL;
            cl_enqueue_pipeline(cl, test->pipeline, test->dst_width / 4, test->dst_height / 2, 1,
                                128, 2, 1, ev);
        }
        if (dispatches == 1)
            end_ev = cl_retain_event(cl, start_ev);

        cl_finish(cl);

        cl_ulong start_ns;
        cl_ulong end_ns;
        cl_get_event_profiling_info(cl, start_ev, CL_PROFILING_COMMAND_START, &start_ns,
                                    sizeof(start_ns));
        cl_get_event_profiling_info(cl, end_ev, CL_PROFILING_COMMAND_END, &end_ns,
                                    sizeof(end_ns));
        cl_destroy_event(cl, start_ev);
        cl_destroy_event(cl, end_ev);

        const float dur_ms = (float)(end_ns - start_ns) / 1000000;
        cl_log("iter %d took %.3f ms", i, dur_ms);
    }
}

int
main(void)
{
    struct tflite_conv_generic_test test = {
        .src_width = 512,
        .src_height = 288,
        .src_slice_count = 6,
        .dst_width = 512,
        .dst_height = 288,
        .dst_slice_count = 2,

        .kernel_width = 3,
        .kernel_height = 3,
        .padding_x = -1,
        .padding_y = -1,
        .stride_x = 1,
        .stride_y = 1,
        .dilation_x = 1,
        .dilation_y = 1,

        .buf_size = 14155776,
        .src_offset = 0,
        .src_size = 7077888,
        .dst_offset = 7077888,
        .dst_size = 2359296,
    };

    tflite_conv_generic_test_init(&test);
    tflite_conv_generic_test_dispatch(&test);
    tflite_conv_generic_test_cleanup(&test);

    return 0;
}
