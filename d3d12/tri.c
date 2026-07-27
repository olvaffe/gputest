/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "d3d12util.h"
#include "tri_test.ps.h"
#include "tri_test.vs.h"

static const struct {
    float pos[2];
    float color[3];
} tri_test_vertices[] = {
    [0] = {
        .pos = { 0.0f, 1.0f, },
        .color = { 1.0f, 0.0f, 0.0f, },
    },
    [1] = {
        .pos = { 1.0f, -1.0f, },
        .color = { 0.0f, 1.0f, 0.0f, },
    },
    [2] = {
        .pos = { -1.0f, -1.0f, },
        .color = { 0.0f, 0.0f, 1.0f, },
    },
};

struct tri_test {
    uint32_t width;
    uint32_t height;
    DXGI_FORMAT format;
    float clear_color[4];

    struct d3d12 d3d12;

    struct d3d12_buffer *vb;
    D3D12_VERTEX_BUFFER_VIEW vbv;

    struct d3d12_image *rt;
    ID3D12DescriptorHeap *rtv_heap;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle;

    struct d3d12_pipeline *pipeline;
};

static void
tri_test_init_pipeline(struct tri_test *test)
{
    struct d3d12 *d3d12 = &test->d3d12;

    test->pipeline = d3d12_create_pipeline(d3d12);

    const D3D12_ROOT_SIGNATURE_DESC root_sig_desc = {
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
    };
    d3d12_add_pipeline_root_signature(d3d12, test->pipeline, &root_sig_desc);

    test->pipeline->input_elements[0] = (D3D12_INPUT_ELEMENT_DESC){
        .SemanticName = "POSITION",
        .Format = DXGI_FORMAT_R32G32_FLOAT,
        .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
    };
    test->pipeline->input_elements[1] = (D3D12_INPUT_ELEMENT_DESC){
        .SemanticName = "COLOR",
        .Format = DXGI_FORMAT_R32G32B32_FLOAT,
        .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
        .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
    };
    test->pipeline->desc.InputLayout.NumElements = 2;

    test->pipeline->desc.VS = (D3D12_SHADER_BYTECODE){
        .pShaderBytecode = tri_test_vs,
        .BytecodeLength = sizeof(tri_test_vs),
    };
    test->pipeline->desc.PS = (D3D12_SHADER_BYTECODE){
        .pShaderBytecode = tri_test_ps,
        .BytecodeLength = sizeof(tri_test_ps),
    };

    test->pipeline->desc.RTVFormats[0] = test->format;

    d3d12_compile_pipeline(d3d12, test->pipeline);
}

static void
tri_test_init_rt(struct tri_test *test)
{
    struct d3d12 *d3d12 = &test->d3d12;

    const D3D12_CLEAR_VALUE clear_val = {
        .Format = test->format,
        .Color = { test->clear_color[0], test->clear_color[1], test->clear_color[2],
                   test->clear_color[3] },
    };
    test->rt = d3d12_create_image(d3d12, test->width, test->height, test->format,
                                  D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                  D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, &clear_val);

    const D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {
        .NumDescriptors = 1,
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    };
    d3d12->result = ID3D12Device_CreateDescriptorHeap(
        d3d12->dev, &rtv_heap_desc, &IID_ID3D12DescriptorHeap, (void **)&test->rtv_heap);
    d3d12_check(d3d12, "CreateDescriptorHeap");

    test->rtv_handle = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(test->rtv_heap);
    ID3D12Device_CreateRenderTargetView(d3d12->dev, test->rt->img, NULL, test->rtv_handle);
}

static void
tri_test_init_vb(struct tri_test *test)
{
    struct d3d12 *d3d12 = &test->d3d12;

    test->vb = d3d12_create_buffer(d3d12, sizeof(tri_test_vertices), D3D12_HEAP_TYPE_UPLOAD,
                                   D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE);

    memcpy(test->vb->map, tri_test_vertices, sizeof(tri_test_vertices));

    test->vbv.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(test->vb->buf);
    test->vbv.StrideInBytes = sizeof(tri_test_vertices[0]);
    test->vbv.SizeInBytes = sizeof(tri_test_vertices);
}

static void
tri_test_init(struct tri_test *test)
{
    struct d3d12 *d3d12 = &test->d3d12;

    d3d12_init(d3d12, NULL);

    tri_test_init_vb(test);
    tri_test_init_rt(test);
    tri_test_init_pipeline(test);
}

static void
tri_test_cleanup(struct tri_test *test)
{
    struct d3d12 *d3d12 = &test->d3d12;

    d3d12_destroy_pipeline(test->pipeline);

    ID3D12DescriptorHeap_Release(test->rtv_heap);
    d3d12_destroy_image(test->rt);

    d3d12_destroy_buffer(test->vb);

    d3d12_cleanup(d3d12);
}

static void
tri_test_draw(struct tri_test *test)
{
    struct d3d12 *d3d12 = &test->d3d12;
    ID3D12GraphicsCommandList *cmd = d3d12_begin_cmd(d3d12);

    ID3D12GraphicsCommandList_ClearRenderTargetView(cmd, test->rtv_handle, test->clear_color, 0,
                                                    NULL);

    ID3D12GraphicsCommandList_SetPipelineState(cmd, test->pipeline->pipeline);
    ID3D12GraphicsCommandList_SetGraphicsRootSignature(cmd, test->pipeline->root_signature);

    ID3D12GraphicsCommandList_IASetVertexBuffers(cmd, 0, 1, &test->vbv);
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(cmd, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const D3D12_VIEWPORT vp = {
        .Width = (float)test->width,
        .Height = (float)test->height,
        .MaxDepth = 1.0f,
    };
    const D3D12_RECT scissor = {
        .right = (LONG)test->width,
        .bottom = (LONG)test->height,
    };
    ID3D12GraphicsCommandList_RSSetViewports(cmd, 1, &vp);
    ID3D12GraphicsCommandList_RSSetScissorRects(cmd, 1, &scissor);

    ID3D12GraphicsCommandList_OMSetRenderTargets(cmd, 1, &test->rtv_handle, FALSE, NULL);

    ID3D12GraphicsCommandList_DrawInstanced(cmd, ARRAY_SIZE(tri_test_vertices), 1, 0, 0);

    d3d12_end_cmd(d3d12, cmd);
    d3d12_wait(d3d12);

    d3d12_dump_image(d3d12, test->rt, D3D12_RESOURCE_STATE_RENDER_TARGET, "rt.ppm");
}

int
main(void)
{
    struct tri_test test = {
        .width = 300,
        .height = 300,
        .format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .clear_color = { 0.0f, 0.2f, 0.4f, 1.0f },
    };

    tri_test_init(&test);
    tri_test_draw(&test);
    tri_test_cleanup(&test);

    return 0;
}
