#pragma OPENCL EXTENSION cl_khr_fp16 : enable

kernel void
loop(global half *dst, uint repeat)
{
    const uint idx = get_global_id(0);
    const half src_val1 = (half)idx;
    const half src_val2 = (half)(idx + 1);

    half dst_val = (half)0;
    __attribute__((opencl_unroll_hint(1)))
    for (uint i = 0; i < repeat; i++) {
        dst_val += src_val1 * src_val2;
    }

    dst[idx] = dst_val;
}
