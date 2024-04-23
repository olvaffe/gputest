#define REDUCE_WIDTH 4
#define REDUCE_HEIGHT 4

#pragma OPENCL EXTENSION cl_khr_fp16 : enable

kernel void
convert(global half4 *dst_buf,
        read_only image1d_buffer_t src_img,
        constant half4 *weight_buf,
        int4 args0,
        int4 args1)
{
    const int width = args0.x;
    const int slice_stride = args0.y;
    const int slice_count = args0.z;
    const int dst_width = args0.w;
    const int kernel_width = args1.x;
    const int kernel_height = args1.y;

    const int base_x = get_global_id(0) * REDUCE_WIDTH;
    const int base_y = get_global_id(1) * REDUCE_HEIGHT;
    const int dst = get_global_id(1) * dst_width + get_global_id(0);

    half4 tmp_vals[REDUCE_HEIGHT][REDUCE_WIDTH] = { 0 };
    for (int y = 0; y < kernel_height; y++) {
        const int ky = base_y + y;
        for (int x = 0; x < kernel_width; x++) {
            const int kx = base_x + x;
            for (int ss = 0; ss < slice_count; ss++) {
                const int base = slice_stride * ss;
                half4 img_vals[REDUCE_HEIGHT][REDUCE_WIDTH];
                img_vals[0][0] = read_imageh(src_img, base + width * (ky + 0) + (kx + 0));
                img_vals[0][1] = read_imageh(src_img, base + width * (ky + 0) + (kx + 1));
                img_vals[0][2] = read_imageh(src_img, base + width * (ky + 0) + (kx + 2));
                img_vals[0][3] = read_imageh(src_img, base + width * (ky + 0) + (kx + 3));
#if REDUCE_HEIGHT >= 2
                img_vals[1][0] = read_imageh(src_img, base + width * (ky + 1) + (kx + 0));
                img_vals[1][1] = read_imageh(src_img, base + width * (ky + 1) + (kx + 1));
                img_vals[1][2] = read_imageh(src_img, base + width * (ky + 1) + (kx + 2));
                img_vals[1][3] = read_imageh(src_img, base + width * (ky + 1) + (kx + 3));
#endif
#if REDUCE_HEIGHT >= 3
                img_vals[2][0] = read_imageh(src_img, base + width * (ky + 2) + (kx + 0));
                img_vals[2][1] = read_imageh(src_img, base + width * (ky + 2) + (kx + 1));
                img_vals[2][2] = read_imageh(src_img, base + width * (ky + 2) + (kx + 2));
                img_vals[2][3] = read_imageh(src_img, base + width * (ky + 2) + (kx + 3));
#endif
#if REDUCE_HEIGHT >= 3
                img_vals[3][0] = read_imageh(src_img, base + width * (ky + 3) + (kx + 0));
                img_vals[3][1] = read_imageh(src_img, base + width * (ky + 3) + (kx + 1));
                img_vals[3][2] = read_imageh(src_img, base + width * (ky + 3) + (kx + 2));
                img_vals[3][3] = read_imageh(src_img, base + width * (ky + 3) + (kx + 3));
#endif

                const int weight_base = (kernel_width * (kernel_height * ss + y) + x) * 4;
                half4 weights[4];
                weights[0] = weight_buf[weight_base + 0];
                weights[1] = weight_buf[weight_base + 1];
                weights[2] = weight_buf[weight_base + 2];
                weights[3] = weight_buf[weight_base + 3];

                tmp_vals[0][0] += weights[0] * img_vals[0][0].x;
                tmp_vals[0][0] += weights[1] * img_vals[0][0].y;
                tmp_vals[0][0] += weights[2] * img_vals[0][0].z;
                tmp_vals[0][0] += weights[3] * img_vals[0][0].w;
                tmp_vals[0][1] += weights[0] * img_vals[0][1].x;
                tmp_vals[0][1] += weights[1] * img_vals[0][1].y;
                tmp_vals[0][1] += weights[2] * img_vals[0][1].z;
                tmp_vals[0][1] += weights[3] * img_vals[0][1].w;
                tmp_vals[0][2] += weights[0] * img_vals[0][2].x;
                tmp_vals[0][2] += weights[1] * img_vals[0][2].y;
                tmp_vals[0][2] += weights[2] * img_vals[0][2].z;
                tmp_vals[0][2] += weights[3] * img_vals[0][2].w;
                tmp_vals[0][3] += weights[0] * img_vals[0][3].x;
                tmp_vals[0][3] += weights[1] * img_vals[0][3].y;
                tmp_vals[0][3] += weights[2] * img_vals[0][3].z;
                tmp_vals[0][3] += weights[3] * img_vals[0][3].w;
#if REDUCE_HEIGHT >= 2
                tmp_vals[1][0] += weights[0] * img_vals[1][0].x;
                tmp_vals[1][0] += weights[1] * img_vals[1][0].y;
                tmp_vals[1][0] += weights[2] * img_vals[1][0].z;
                tmp_vals[1][0] += weights[3] * img_vals[1][0].w;
                tmp_vals[1][1] += weights[0] * img_vals[1][1].x;
                tmp_vals[1][1] += weights[1] * img_vals[1][1].y;
                tmp_vals[1][1] += weights[2] * img_vals[1][1].z;
                tmp_vals[1][1] += weights[3] * img_vals[1][1].w;
                tmp_vals[1][2] += weights[0] * img_vals[1][2].x;
                tmp_vals[1][2] += weights[1] * img_vals[1][2].y;
                tmp_vals[1][2] += weights[2] * img_vals[1][2].z;
                tmp_vals[1][2] += weights[3] * img_vals[1][2].w;
                tmp_vals[1][3] += weights[0] * img_vals[1][3].x;
                tmp_vals[1][3] += weights[1] * img_vals[1][3].y;
                tmp_vals[1][3] += weights[2] * img_vals[1][3].z;
                tmp_vals[1][3] += weights[3] * img_vals[1][3].w;
#endif
#if REDUCE_HEIGHT >= 3
                tmp_vals[2][0] += weights[0] * img_vals[2][0].x;
                tmp_vals[2][0] += weights[1] * img_vals[2][0].y;
                tmp_vals[2][0] += weights[2] * img_vals[2][0].z;
                tmp_vals[2][0] += weights[3] * img_vals[2][0].w;
                tmp_vals[2][1] += weights[0] * img_vals[2][1].x;
                tmp_vals[2][1] += weights[1] * img_vals[2][1].y;
                tmp_vals[2][1] += weights[2] * img_vals[2][1].z;
                tmp_vals[2][1] += weights[3] * img_vals[2][1].w;
                tmp_vals[2][2] += weights[0] * img_vals[2][2].x;
                tmp_vals[2][2] += weights[1] * img_vals[2][2].y;
                tmp_vals[2][2] += weights[2] * img_vals[2][2].z;
                tmp_vals[2][2] += weights[3] * img_vals[2][2].w;
                tmp_vals[2][3] += weights[0] * img_vals[2][3].x;
                tmp_vals[2][3] += weights[1] * img_vals[2][3].y;
                tmp_vals[2][3] += weights[2] * img_vals[2][3].z;
                tmp_vals[2][3] += weights[3] * img_vals[2][3].w;
#endif
#if REDUCE_HEIGHT >= 4
                tmp_vals[3][0] += weights[0] * img_vals[3][0].x;
                tmp_vals[3][0] += weights[1] * img_vals[3][0].y;
                tmp_vals[3][0] += weights[2] * img_vals[3][0].z;
                tmp_vals[3][0] += weights[3] * img_vals[3][0].w;
                tmp_vals[3][1] += weights[0] * img_vals[3][1].x;
                tmp_vals[3][1] += weights[1] * img_vals[3][1].y;
                tmp_vals[3][1] += weights[2] * img_vals[3][1].z;
                tmp_vals[3][1] += weights[3] * img_vals[3][1].w;
                tmp_vals[3][2] += weights[0] * img_vals[3][2].x;
                tmp_vals[3][2] += weights[1] * img_vals[3][2].y;
                tmp_vals[3][2] += weights[2] * img_vals[3][2].z;
                tmp_vals[3][2] += weights[3] * img_vals[3][2].w;
                tmp_vals[3][3] += weights[0] * img_vals[3][3].x;
                tmp_vals[3][3] += weights[1] * img_vals[3][3].y;
                tmp_vals[3][3] += weights[2] * img_vals[3][3].z;
                tmp_vals[3][3] += weights[3] * img_vals[3][3].w;
#endif
            }
        }
    }

    half4 sum = (half4)0;
    sum += tmp_vals[0][0] + tmp_vals[0][1] + tmp_vals[0][2] + tmp_vals[0][3];
#if REDUCE_HEIGHT >= 2
    sum += tmp_vals[1][0] + tmp_vals[1][1] + tmp_vals[1][2] + tmp_vals[1][3];
#endif
#if REDUCE_HEIGHT >= 3
    sum += tmp_vals[2][0] + tmp_vals[2][1] + tmp_vals[2][2] + tmp_vals[2][3];
#endif
#if REDUCE_HEIGHT >= 4
    sum += tmp_vals[3][0] + tmp_vals[3][1] + tmp_vals[3][2] + tmp_vals[3][3];
#endif

    dst_buf[dst] = sum;
}
