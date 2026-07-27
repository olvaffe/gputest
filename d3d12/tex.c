/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "d3d12util.h"
#include "tex_ps.h"
#include "tex_vs.h"

static const unsigned char tex_test_ppm[] = {
#include "tex_test.ppm.inc"
};

static const struct {
    float pos[2];
    float uv[2];
} tex_test_vertices[] = {
    [0] = {
        .pos = { -1.0f, -1.0f },
        .uv = { 0.0f, 0.0f },
    },
    [1] = {
        .pos = { 1.0f, -1.0f },
        .uv = { 1.0f, 0.0f },
    },
    [2] = {
        .pos = { -1.0f, 1.0f },
        .uv = { 0.0f, 1.0f },
    },
    [3] = {
        .pos = { 1.0f, 1.0f },
        .uv = { 1.0f, 1.0f },
    },
};

struct tex_test {
    uint32_t width;
    uint32_t height;
    DXGI_FORMAT format;

    struct d3d12 d3d12;

    struct d3d12_buffer *vb;
    D3D12_VERTEX_BUFFER_VIEW vbv;

    struct d3d12_image *tex;
    ID3D12DescriptorHeap *srv_heap;
    D3D12_CPU_DESCRIPTOR_HANDLE srv_handle;

    struct d3d12_image *rt;
    ID3D12DescriptorHeap *rtv_heap;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle;

    struct d3d12_pipeline *pipeline;
};

static void
tex_test_init_pipeline(struct tex_test *test)
{
    struct d3d12 *d3d12 = &test->d3d12;

    test->pipeline = d3d12_create_pipeline(d3d12);

    const D3D12_DESCRIPTOR_RANGE srv_range = {
        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        .NumDescriptors = 1,
        .BaseShaderRegister = 0,
        .RegisterSpace = 0,
        .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
    };

    const D3D12_ROOT_PARAMETER root_param = {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
        .DescriptorTable = {
            .NumDescriptorRanges = 1,
            .pDescriptorRanges = &srv_range,
        },
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
    };

    const D3D12_STATIC_SAMPLER_DESC static_sampler = {
        .Filter = D3D12_FILTER_MIN_MAG_MIP_POINT,
        .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .MipLODBias = 0.0f,
        .MaxAnisotropy = 1,
        .ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS,
        .BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
        .MinLOD = 0.0f,
        .MaxLOD = D3D12_FLOAT32_MAX,
        .ShaderRegister = 0,
        .RegisterSpace = 0,
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
    };

    const D3D12_ROOT_SIGNATURE_DESC root_sig_desc = {
        .NumParameters = 1,
        .pParameters = &root_param,
        .NumStaticSamplers = 1,
        .pStaticSamplers = &static_sampler,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
    };

    d3d12_add_pipeline_root_signature(d3d12, test->pipeline, &root_sig_desc);

    test->pipeline->input_elements[0] = (D3D12_INPUT_ELEMENT_DESC){
        .SemanticName = "POSITION",
        .Format = DXGI_FORMAT_R32G32_FLOAT,
        .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
    };
    test->pipeline->input_elements[1] = (D3D12_INPUT_ELEMENT_DESC){
        .SemanticName = "TEXCOORD",
        .Format = DXGI_FORMAT_R32G32_FLOAT,
        .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
        .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
    };
    test->pipeline->desc.InputLayout.NumElements = 2;

    test->pipeline->desc.VS = (D3D12_SHADER_BYTECODE){
        .pShaderBytecode = tex_test_vs,
        .BytecodeLength = sizeof(tex_test_vs),
    };
    test->pipeline->desc.PS = (D3D12_SHADER_BYTECODE){
        .pShaderBytecode = tex_test_ps,
        .BytecodeLength = sizeof(tex_test_ps),
    };

    test->pipeline->desc.RTVFormats[0] = test->format;

    d3d12_compile_pipeline(d3d12, test->pipeline);
}

static void
tex_test_init_rt(struct tex_test *test)
{
    struct d3d12 *d3d12 = &test->d3d12;

    test->rt = d3d12_create_image(d3d12, test->width, test->height, test->format,
                                  D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                  D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, NULL);

    const D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {
        .NumDescriptors = 1,
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    };
    d3d12->result = ID3D12Device_CreateDescriptorHeap(
        d3d12->dev, &rtv_heap_desc, &IID_ID3D12DescriptorHeap, (void **)&test->rtv_heap);
    d3d12_check(d3d12, "CreateDescriptorHeap (RTV)");

    test->rtv_handle = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(test->rtv_heap);
    ID3D12Device_CreateRenderTargetView(d3d12->dev, test->rt->img, NULL, test->rtv_handle);
}

static void
tex_test_init_tex(struct tex_test *test)
{
    struct d3d12 *d3d12 = &test->d3d12;

    test->tex = d3d12_create_image_from_ppm(d3d12, tex_test_ppm, sizeof(tex_test_ppm));

    const D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc = {
        .NumDescriptors = 1,
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
    };
    d3d12->result = ID3D12Device_CreateDescriptorHeap(
        d3d12->dev, &srv_heap_desc, &IID_ID3D12DescriptorHeap, (void **)&test->srv_heap);
    d3d12_check(d3d12, "CreateDescriptorHeap (SRV)");

    test->srv_handle = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(test->srv_heap);

    const D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
        .Format = test->tex->format,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D = {
            .MostDetailedMip = 0,
            .MipLevels = 1,
            .PlaneSlice = 0,
            .ResourceMinLODClamp = 0.0f,
        },
    };
    ID3D12Device_CreateShaderResourceView(d3d12->dev, test->tex->img, &srv_desc,
                                          test->srv_handle);
}

static void
tex_test_init_vb(struct tex_test *test)
{
    struct d3d12 *d3d12 = &test->d3d12;

    test->vb = d3d12_create_buffer(d3d12, sizeof(tex_test_vertices), D3D12_HEAP_TYPE_UPLOAD,
                                   D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE);

    memcpy(test->vb->map, tex_test_vertices, sizeof(tex_test_vertices));

    test->vbv.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(test->vb->buf);
    test->vbv.StrideInBytes = sizeof(tex_test_vertices[0]);
    test->vbv.SizeInBytes = sizeof(tex_test_vertices);
}

static void
tex_test_init(struct tex_test *test)
{
    struct d3d12 *d3d12 = &test->d3d12;

    d3d12_init(d3d12, NULL);

    tex_test_init_vb(test);
    tex_test_init_tex(test);
    tex_test_init_rt(test);
    tex_test_init_pipeline(test);
}

static void
tex_test_cleanup(struct tex_test *test)
{
    struct d3d12 *d3d12 = &test->d3d12;

    d3d12_destroy_pipeline(test->pipeline);

    ID3D12DescriptorHeap_Release(test->srv_heap);
    d3d12_destroy_image(test->tex);

    ID3D12DescriptorHeap_Release(test->rtv_heap);
    d3d12_destroy_image(test->rt);

    d3d12_destroy_buffer(test->vb);

    d3d12_cleanup(d3d12);
}

static void
tex_test_draw(struct tex_test *test)
{
    struct d3d12 *d3d12 = &test->d3d12;
    ID3D12GraphicsCommandList *cmd = d3d12_begin_cmd(d3d12);

    const D3D12_VIEWPORT vp = {
        .Width = (float)test->width,
        .Height = (float)test->height,
        .MaxDepth = 1.0f,
    };
    const D3D12_RECT scissor = {
        .right = (LONG)test->width,
        .bottom = (LONG)test->height,
    };

    ID3D12GraphicsCommandList_IASetVertexBuffers(cmd, 0, 1, &test->vbv);
    ID3D12GraphicsCommandList_SetPipelineState(cmd, test->pipeline->pipeline);
    ID3D12GraphicsCommandList_SetGraphicsRootSignature(cmd, test->pipeline->root_signature);

    ID3D12GraphicsCommandList_SetDescriptorHeaps(cmd, 1, &test->srv_heap);
    const D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle =
        ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(test->srv_heap);
    ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(cmd, 0, gpu_handle);

    ID3D12GraphicsCommandList_IASetPrimitiveTopology(cmd, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ID3D12GraphicsCommandList_RSSetViewports(cmd, 1, &vp);
    ID3D12GraphicsCommandList_RSSetScissorRects(cmd, 1, &scissor);
    ID3D12GraphicsCommandList_OMSetRenderTargets(cmd, 1, &test->rtv_handle, FALSE, NULL);

    ID3D12GraphicsCommandList_DrawInstanced(cmd, ARRAY_SIZE(tex_test_vertices), 1, 0, 0);

    d3d12_end_cmd(d3d12, cmd);
    d3d12_wait(d3d12);

    d3d12_dump_image(d3d12, test->tex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, "tex.ppm");
    d3d12_dump_image(d3d12, test->rt, D3D12_RESOURCE_STATE_RENDER_TARGET, "rt.ppm");
}

int
main(void)
{
    struct tex_test test = {
        .width = 300,
        .height = 300,
        .format = DXGI_FORMAT_R8G8B8A8_UNORM,
    };

    tex_test_init(&test);
    tex_test_draw(&test);
    tex_test_cleanup(&test);

    return 0;
}
