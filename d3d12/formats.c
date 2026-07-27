/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "d3d12util.h"

struct formats_test_format {
    DXGI_FORMAT format;
    const char *name;
};

struct formats_bit_name {
    uint64_t bit;
    const char *name;
};

static const struct formats_test_format formats_test_formats[] = {
#define FMT(fmt) { DXGI_FORMAT_##fmt, "DXGI_FORMAT_" #fmt }
    FMT(UNKNOWN),
    FMT(R32G32B32A32_TYPELESS),
    FMT(R32G32B32A32_FLOAT),
    FMT(R32G32B32A32_UINT),
    FMT(R32G32B32A32_SINT),
    FMT(R32G32B32_TYPELESS),
    FMT(R32G32B32_FLOAT),
    FMT(R32G32B32_UINT),
    FMT(R32G32B32_SINT),
    FMT(R16G16B16A16_TYPELESS),
    FMT(R16G16B16A16_FLOAT),
    FMT(R16G16B16A16_UNORM),
    FMT(R16G16B16A16_UINT),
    FMT(R16G16B16A16_SNORM),
    FMT(R16G16B16A16_SINT),
    FMT(R32G32_TYPELESS),
    FMT(R32G32_FLOAT),
    FMT(R32G32_UINT),
    FMT(R32G32_SINT),
    FMT(R32G8X24_TYPELESS),
    FMT(D32_FLOAT_S8X24_UINT),
    FMT(R32_FLOAT_X8X24_TYPELESS),
    FMT(X32_TYPELESS_G8X24_UINT),
    FMT(R10G10B10A2_TYPELESS),
    FMT(R10G10B10A2_UNORM),
    FMT(R10G10B10A2_UINT),
    FMT(R11G11B10_FLOAT),
    FMT(R8G8B8A8_TYPELESS),
    FMT(R8G8B8A8_UNORM),
    FMT(R8G8B8A8_UNORM_SRGB),
    FMT(R8G8B8A8_UINT),
    FMT(R8G8B8A8_SNORM),
    FMT(R8G8B8A8_SINT),
    FMT(R16G16_TYPELESS),
    FMT(R16G16_FLOAT),
    FMT(R16G16_UNORM),
    FMT(R16G16_UINT),
    FMT(R16G16_SNORM),
    FMT(R16G16_SINT),
    FMT(R32_TYPELESS),
    FMT(D32_FLOAT),
    FMT(R32_FLOAT),
    FMT(R32_UINT),
    FMT(R32_SINT),
    FMT(R24G8_TYPELESS),
    FMT(D24_UNORM_S8_UINT),
    FMT(R24_UNORM_X8_TYPELESS),
    FMT(X24_TYPELESS_G8_UINT),
    FMT(R8G8_TYPELESS),
    FMT(R8G8_UNORM),
    FMT(R8G8_UINT),
    FMT(R8G8_SNORM),
    FMT(R8G8_SINT),
    FMT(R16_TYPELESS),
    FMT(R16_FLOAT),
    FMT(D16_UNORM),
    FMT(R16_UNORM),
    FMT(R16_UINT),
    FMT(R16_SNORM),
    FMT(R16_SINT),
    FMT(R8_TYPELESS),
    FMT(R8_UNORM),
    FMT(R8_UINT),
    FMT(R8_SNORM),
    FMT(R8_SINT),
    FMT(A8_UNORM),
    FMT(R1_UNORM),
    FMT(R9G9B9E5_SHAREDEXP),
    FMT(R8G8_B8G8_UNORM),
    FMT(G8R8_G8B8_UNORM),
    FMT(BC1_TYPELESS),
    FMT(BC1_UNORM),
    FMT(BC1_UNORM_SRGB),
    FMT(BC2_TYPELESS),
    FMT(BC2_UNORM),
    FMT(BC2_UNORM_SRGB),
    FMT(BC3_TYPELESS),
    FMT(BC3_UNORM),
    FMT(BC3_UNORM_SRGB),
    FMT(BC4_TYPELESS),
    FMT(BC4_UNORM),
    FMT(BC4_SNORM),
    FMT(BC5_TYPELESS),
    FMT(BC5_UNORM),
    FMT(BC5_SNORM),
    FMT(B5G6R5_UNORM),
    FMT(B5G5R5A1_UNORM),
    FMT(B8G8R8A8_UNORM),
    FMT(B8G8R8X8_UNORM),
    FMT(R10G10B10_XR_BIAS_A2_UNORM),
    FMT(B8G8R8A8_TYPELESS),
    FMT(B8G8R8A8_UNORM_SRGB),
    FMT(B8G8R8X8_TYPELESS),
    FMT(B8G8R8X8_UNORM_SRGB),
    FMT(BC6H_TYPELESS),
    FMT(BC6H_UF16),
    FMT(BC6H_SF16),
    FMT(BC7_TYPELESS),
    FMT(BC7_UNORM),
    FMT(BC7_UNORM_SRGB),
    FMT(AYUV),
    FMT(Y410),
    FMT(Y416),
    FMT(NV12),
    FMT(P010),
    FMT(P016),
    FMT(420_OPAQUE),
    FMT(YUY2),
    FMT(Y210),
    FMT(Y216),
    FMT(NV11),
    FMT(AI44),
    FMT(IA44),
    FMT(P8),
    FMT(A8P8),
    FMT(B4G4R4A4_UNORM),
    FMT(P208),
    FMT(V208),
    FMT(V408),
    FMT(SAMPLER_FEEDBACK_MIN_MIP_OPAQUE),
    FMT(SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE),
    FMT(A4B4G4R4_UNORM),
#undef FMT
};

static const struct formats_bit_name format_support1_names[] = {
    { D3D12_FORMAT_SUPPORT1_BUFFER, "buffer" },
    { D3D12_FORMAT_SUPPORT1_IA_VERTEX_BUFFER, "vertex" },
    { D3D12_FORMAT_SUPPORT1_IA_INDEX_BUFFER, "index" },
    { D3D12_FORMAT_SUPPORT1_SO_BUFFER, "stream_output" },
    { D3D12_FORMAT_SUPPORT1_TEXTURE1D, "tex1d" },
    { D3D12_FORMAT_SUPPORT1_TEXTURE2D, "tex2d" },
    { D3D12_FORMAT_SUPPORT1_TEXTURE3D, "tex3d" },
    { D3D12_FORMAT_SUPPORT1_TEXTURECUBE, "texcube" },
    { D3D12_FORMAT_SUPPORT1_SHADER_LOAD, "shader_load" },
    { D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE, "shader_sample" },
    { D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE_COMPARISON, "shader_sample_cmp" },
    { D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE_MONO_TEXT, "shader_sample_mono" },
    { D3D12_FORMAT_SUPPORT1_MIP, "mip" },
    { D3D12_FORMAT_SUPPORT1_RENDER_TARGET, "rtv" },
    { D3D12_FORMAT_SUPPORT1_BLENDABLE, "blendable" },
    { D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL, "dsv" },
    { D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RESOLVE, "msaa_resolve" },
    { D3D12_FORMAT_SUPPORT1_DISPLAY, "display" },
    { D3D12_FORMAT_SUPPORT1_CAST_WITHIN_BIT_LAYOUT, "cast_bit_layout" },
    { D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RENDERTARGET, "msaa_rtv" },
    { D3D12_FORMAT_SUPPORT1_MULTISAMPLE_LOAD, "msaa_load" },
    { D3D12_FORMAT_SUPPORT1_SHADER_GATHER, "shader_gather" },
    { D3D12_FORMAT_SUPPORT1_BACK_BUFFER_CAST, "back_buffer_cast" },
    { D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW, "uav" },
    { D3D12_FORMAT_SUPPORT1_SHADER_GATHER_COMPARISON, "shader_gather_cmp" },
    { D3D12_FORMAT_SUPPORT1_DECODER_OUTPUT, "decoder_output" },
    { D3D12_FORMAT_SUPPORT1_VIDEO_PROCESSOR_OUTPUT, "video_proc_output" },
    { D3D12_FORMAT_SUPPORT1_VIDEO_PROCESSOR_INPUT, "video_proc_input" },
    { D3D12_FORMAT_SUPPORT1_VIDEO_ENCODER, "video_encoder" },
};

static const struct formats_bit_name format_support2_names[] = {
    { D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_ADD, "uav_atomic_add" },
    { D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_BITWISE_OPS, "uav_atomic_bitwise" },
    { D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_COMPARE_STORE_OR_COMPARE_EXCHANGE, "uav_atomic_cmpxchg" },
    { D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_EXCHANGE, "uav_atomic_xchg" },
    { D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_SIGNED_MIN_OR_MAX, "uav_atomic_sminmax" },
    { D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_UNSIGNED_MIN_OR_MAX, "uav_atomic_uminmax" },
    { D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD, "uav_typed_load" },
    { D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE, "uav_typed_store" },
    { D3D12_FORMAT_SUPPORT2_OUTPUT_MERGER_LOGIC_OP, "logic_op" },
    { D3D12_FORMAT_SUPPORT2_TILED, "tiled" },
    { D3D12_FORMAT_SUPPORT2_MULTIPLANE_OVERLAY, "multiplane_overlay" },
    { D3D12_FORMAT_SUPPORT2_SAMPLER_FEEDBACK, "sampler_feedback" },
};

static void
formats_get_support_str(
    uint64_t bits, const struct formats_bit_name *names, size_t count, char *buf, size_t size)
{
    size_t len = 0;
    for (size_t i = 0; i < count; i++) {
        if (bits & names[i].bit) {
            len += snprintf(buf + len, size - len, "%s|", names[i].name);
            bits &= ~names[i].bit;
        }
    }
    if (bits) {
        snprintf(buf + len, size - len, "0x%" PRIx64, bits);
    } else if (len > 0) {
        buf[len - 1] = '\0';
    } else {
        snprintf(buf, size, "none");
    }
}

static void
formats_test_dump_format(struct d3d12 *d3d12, const struct formats_test_format *fmt)
{
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support = { .Format = fmt->format };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_FORMAT_SUPPORT,
                                                     &support, sizeof(support));
    if (FAILED(d3d12->result) || (support.Support1 == D3D12_FORMAT_SUPPORT1_NONE &&
                                  support.Support2 == D3D12_FORMAT_SUPPORT2_NONE)) {
        d3d12_log("%s is not supported", fmt->name);
        return;
    }

    d3d12_log("%s", fmt->name);

    char str1[512];
    char str2[256];
    formats_get_support_str(support.Support1, format_support1_names,
                            ARRAY_SIZE(format_support1_names), str1, sizeof(str1));
    formats_get_support_str(support.Support2, format_support2_names,
                            ARRAY_SIZE(format_support2_names), str2, sizeof(str2));

    d3d12_log("  Support1: %s", str1);
    d3d12_log("  Support2: %s", str2);

    D3D12_FEATURE_DATA_FORMAT_INFO info = { .Format = fmt->format };
    if (SUCCEEDED(ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_FORMAT_INFO, &info,
                                                   sizeof(info)))) {
        d3d12_log("  PlaneCount: %u", info.PlaneCount);
    }
}

static void
formats_test_dump(struct d3d12 *d3d12)
{
    for (size_t i = 0; i < ARRAY_SIZE(formats_test_formats); i++) {
        formats_test_dump_format(d3d12, &formats_test_formats[i]);
    }
}

int
main(void)
{
    struct d3d12 d3d12;

    d3d12_init(&d3d12, NULL);
    formats_test_dump(&d3d12);
    d3d12_cleanup(&d3d12);

    return 0;
}
