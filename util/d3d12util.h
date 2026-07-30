/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef D3D12UTIL_H
#define D3D12UTIL_H

#include "util.h"

#include <dlfcn.h>
#include <sys/eventfd.h>

#define INITGUID
#include <vkd3d_windows.h>
#define COBJMACROS
#define WIDL_C_INLINE_WRAPPERS
#include <vkd3d_d3d12.h>
#include <vkd3d_device_vkd3d_ext.h>
#include <vkd3d_dxgi.h>

#define LIBVKD3D_PROTON_D3D12_NAME "libvkd3d-proton-d3d12.so"

#define d3d12_log(format, ...) u_log("D3D12", format __VA_OPT__(, ) __VA_ARGS__)
#define d3d12_die(format, ...) u_die("D3D12", format __VA_OPT__(, ) __VA_ARGS__)

struct d3d12_init_params {
    IDXGIAdapter *adapter;
    D3D_FEATURE_LEVEL feature_level;
};

struct d3d12 {
    struct d3d12_init_params params;

    struct {
        void *handle;

#define PFN_D3D12(type, name) PFN_D3D12_##type name;
#include "d3d12util_entrypoints.inc"
    };

    HRESULT result;

    ID3D12Device *dev;
    UINT dev_version;

    ID3D12CommandQueue *queue;
    ID3D12Fence *fence;
    UINT64 fence_val;
    int fence_event;

    ID3D12CommandAllocator *cmd_alloc;
};

struct d3d12_buffer {
    UINT64 size;
    D3D12_HEAP_TYPE heap_type;
    D3D12_RESOURCE_FLAGS flags;

    ID3D12Resource *buf;
    void *map;
};

struct d3d12_image {
    uint32_t width;
    uint32_t height;
    DXGI_FORMAT format;
    D3D12_HEAP_TYPE heap_type;
    D3D12_RESOURCE_FLAGS flags;

    ID3D12Resource *img;

    D3D12_RESOURCE_DESC desc;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT64 total_bytes;
};

struct d3d12_pipeline {
    ID3D12RootSignature *root_signature;

    D3D12_INPUT_ELEMENT_DESC input_elements[16];
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;

    ID3D12PipelineState *pipeline;
};

#define d3d12_check(d3d12, msg)                                                                  \
    u_check("D3D12", SUCCEEDED((d3d12)->result), "%s failed: 0x%08x", msg,                       \
            (unsigned)(d3d12)->result)

static inline void
d3d12_init_library(struct d3d12 *d3d12)
{
    d3d12->handle = dlopen(LIBVKD3D_PROTON_D3D12_NAME, RTLD_LOCAL | RTLD_LAZY);
    if (!d3d12->handle)
        d3d12_die("failed to load %s: %s", LIBVKD3D_PROTON_D3D12_NAME, dlerror());

#define PFN_D3D12(type, name)                                                                    \
    d3d12->name = (PFN_D3D12_##type)dlsym(d3d12->handle, "D3D12" #name);                         \
    if (!d3d12->name)                                                                            \
        d3d12_die("no D3D12" #name);
#include "d3d12util_entrypoints.inc"
}

static inline void
d3d12_init_dev(struct d3d12 *d3d12)
{
    const struct {
        const GUID *iid;
        UINT version;
    } versions[] = {
        { &IID_ID3D12Device14, 14 }, { &IID_ID3D12Device13, 13 }, { &IID_ID3D12Device12, 12 },
        { &IID_ID3D12Device11, 11 }, { &IID_ID3D12Device10, 10 }, { &IID_ID3D12Device9, 9 },
        { &IID_ID3D12Device8, 8 },   { &IID_ID3D12Device7, 7 },   { &IID_ID3D12Device6, 6 },
        { &IID_ID3D12Device5, 5 },   { &IID_ID3D12Device4, 4 },   { &IID_ID3D12Device3, 3 },
        { &IID_ID3D12Device2, 2 },   { &IID_ID3D12Device1, 1 },   { &IID_ID3D12Device, 0 },
    };

    for (size_t i = 0; i < ARRAY_SIZE(versions); i++) {
        HRESULT hr =
            d3d12->CreateDevice((IUnknown *)d3d12->params.adapter, d3d12->params.feature_level,
                                versions[i].iid, (void **)&d3d12->dev);
        if (SUCCEEDED(hr)) {
            d3d12->dev_version = versions[i].version;
            break;
        }
        if (hr != E_NOINTERFACE)
            d3d12_die("D3D12CreateDevice failed: 0x%08x", (unsigned)hr);
    }

    if (!d3d12->dev)
        d3d12_die("CreateDevice failed");
}

static inline void
d3d12_init_queue(struct d3d12 *d3d12)
{
    const D3D12_COMMAND_QUEUE_DESC queue_desc = {
        .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
    };
    d3d12->result = ID3D12Device_CreateCommandQueue(
        d3d12->dev, &queue_desc, &IID_ID3D12CommandQueue, (void **)&d3d12->queue);
    d3d12_check(d3d12, "CreateCommandQueue");

    d3d12->result = ID3D12Device_CreateFence(d3d12->dev, d3d12->fence_val, D3D12_FENCE_FLAG_NONE,
                                             &IID_ID3D12Fence, (void **)&d3d12->fence);
    d3d12_check(d3d12, "CreateFence");

    d3d12->fence_event = eventfd(0, EFD_CLOEXEC);
    if (d3d12->fence_event < 0)
        d3d12_die("eventfd failed");
}

static inline void
d3d12_init_cmd(struct d3d12 *d3d12)
{
    d3d12->result = ID3D12Device_CreateCommandAllocator(
        d3d12->dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator,
        (void **)&d3d12->cmd_alloc);
    d3d12_check(d3d12, "CreateCommandAllocator");
}

static inline void
d3d12_init(struct d3d12 *d3d12, const struct d3d12_init_params *params)
{
    memset(d3d12, 0, sizeof(*d3d12));
    d3d12->fence_event = -1;

    if (params)
        d3d12->params = *params;
    if (!d3d12->params.feature_level)
        d3d12->params.feature_level = D3D_FEATURE_LEVEL_11_0;

    d3d12_init_library(d3d12);
    d3d12_init_dev(d3d12);
    d3d12_init_queue(d3d12);
    d3d12_init_cmd(d3d12);
}

static inline void
d3d12_wait(struct d3d12 *d3d12)
{
    const UINT64 v = ++d3d12->fence_val;
    d3d12->result = ID3D12CommandQueue_Signal(d3d12->queue, d3d12->fence, v);
    d3d12_check(d3d12, "Signal");

    if (ID3D12Fence_GetCompletedValue(d3d12->fence) < v) {
        d3d12->result = ID3D12Fence_SetEventOnCompletion(d3d12->fence, v,
                                                         (HANDLE)(intptr_t)d3d12->fence_event);
        d3d12_check(d3d12, "SetEventOnCompletion");

        uint64_t ev_val;
        if (read(d3d12->fence_event, &ev_val, sizeof(ev_val)) < 0)
            d3d12_die("read eventfd failed");
    }
}

static inline void
d3d12_cleanup(struct d3d12 *d3d12)
{
    d3d12_wait(d3d12);

    ID3D12CommandAllocator_Release(d3d12->cmd_alloc);

    close(d3d12->fence_event);
    ID3D12Fence_Release(d3d12->fence);
    ID3D12CommandQueue_Release(d3d12->queue);

    ID3D12Device_Release(d3d12->dev);

    dlclose(d3d12->handle);
}

static inline ID3D12GraphicsCommandList *
d3d12_begin_cmd(struct d3d12 *d3d12)
{
    ID3D12GraphicsCommandList *cmd;

    d3d12->result = ID3D12Device_CreateCommandList(d3d12->dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                   d3d12->cmd_alloc, NULL,
                                                   &IID_ID3D12GraphicsCommandList, (void **)&cmd);
    d3d12_check(d3d12, "CreateCommandList");

    return cmd;
}

static inline void
d3d12_end_cmd(struct d3d12 *d3d12, ID3D12GraphicsCommandList *cmd)
{
    d3d12->result = ID3D12GraphicsCommandList_Close(cmd);
    d3d12_check(d3d12, "Close");

    ID3D12CommandQueue_ExecuteCommandLists(d3d12->queue, 1, (ID3D12CommandList **)&cmd);
    ID3D12GraphicsCommandList_Release(cmd);
}

static inline struct d3d12_buffer *
d3d12_create_buffer(struct d3d12 *d3d12,
                    UINT64 size,
                    D3D12_HEAP_TYPE heap_type,
                    D3D12_RESOURCE_STATES initial_state,
                    D3D12_RESOURCE_FLAGS flags)
{
    struct d3d12_buffer *buf = (struct d3d12_buffer *)calloc(1, sizeof(*buf));
    if (!buf)
        d3d12_die("failed to allocate buffer");

    buf->size = size;
    buf->heap_type = heap_type;
    buf->flags = flags;

    const D3D12_HEAP_PROPERTIES heap_props = {
        .Type = heap_type,
    };
    const D3D12_RESOURCE_DESC res_desc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Width = size,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {
            .Count = 1,
            .Quality = 0,
        },
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = flags,
    };

    d3d12->result = ID3D12Device_CreateCommittedResource(
        d3d12->dev, &heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, initial_state, NULL,
        &IID_ID3D12Resource, (void **)&buf->buf);
    d3d12_check(d3d12, "CreateCommittedResource (Buffer)");

    if (heap_type == D3D12_HEAP_TYPE_UPLOAD || heap_type == D3D12_HEAP_TYPE_READBACK ||
        heap_type == D3D12_HEAP_TYPE_GPU_UPLOAD) {
        d3d12->result = ID3D12Resource_Map(buf->buf, 0, NULL, &buf->map);
        d3d12_check(d3d12, "Map Buffer");
    }

    return buf;
}

static inline void
d3d12_destroy_buffer(struct d3d12_buffer *buf)
{
    if (buf->map)
        ID3D12Resource_Unmap(buf->buf, 0, NULL);

    ID3D12Resource_Release(buf->buf);
    free(buf);
}

static inline struct d3d12_image *
d3d12_create_image(struct d3d12 *d3d12,
                   uint32_t width,
                   uint32_t height,
                   DXGI_FORMAT format,
                   D3D12_HEAP_TYPE heap_type,
                   D3D12_RESOURCE_STATES initial_state,
                   D3D12_RESOURCE_FLAGS flags,
                   const D3D12_CLEAR_VALUE *clear_val)
{
    struct d3d12_image *img = (struct d3d12_image *)calloc(1, sizeof(*img));
    if (!img)
        d3d12_die("failed to allocate image");

    img->width = width;
    img->height = height;
    img->format = format;
    img->heap_type = heap_type;
    img->flags = flags;

    const D3D12_HEAP_PROPERTIES heap_props = { .Type = heap_type };
    const D3D12_RESOURCE_DESC res_desc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Width = width,
        .Height = height,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = format,
        .SampleDesc = {
            .Count = 1,
            .Quality = 0,
        },
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = flags,
    };

    d3d12->result = ID3D12Device_CreateCommittedResource(
        d3d12->dev, &heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, initial_state, clear_val,
        &IID_ID3D12Resource, (void **)&img->img);
    d3d12_check(d3d12, "CreateCommittedResource (Image)");

    img->desc = ID3D12Resource_GetDesc(img->img);
    UINT num_rows;
    UINT64 row_size;
    ID3D12Device_GetCopyableFootprints(d3d12->dev, &img->desc, 0, 1, 0, &img->footprint,
                                       &num_rows, &row_size, &img->total_bytes);

    return img;
}

static inline void
d3d12_destroy_image(struct d3d12_image *img)
{
    ID3D12Resource_Release(img->img);
    free(img);
}

static inline struct d3d12_image *
d3d12_create_image_from_ppm(struct d3d12 *d3d12, const void *ppm_data, size_t ppm_size)
{
    uint32_t width;
    uint32_t height;
    ppm_data = u_parse_ppm(ppm_data, ppm_size, &width, &height);

    struct d3d12_image *img = d3d12_create_image(
        d3d12, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE, NULL);

    struct d3d12_buffer *upload_buf =
        d3d12_create_buffer(d3d12, img->total_bytes, D3D12_HEAP_TYPE_UPLOAD,
                            D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE);

    const struct u_format_conversion conv = {
        .width = width,
        .height = height,
        .src_format = DRM_FORMAT_BGR888,
        .src_plane_count = 1,
        .src_plane_ptrs = { ppm_data, },
        .src_plane_strides = { width * 3, },
        .dst_format = DRM_FORMAT_ABGR8888,
        .dst_plane_count = 1,
        .dst_plane_ptrs = { (uint8_t *)upload_buf->map + img->footprint.Offset, },
        .dst_plane_strides = { img->footprint.Footprint.RowPitch, },
    };
    u_convert_format(&conv);

    ID3D12GraphicsCommandList *cmd = d3d12_begin_cmd(d3d12);

    const D3D12_TEXTURE_COPY_LOCATION dst_loc = {
        .pResource = img->img,
        .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        .SubresourceIndex = 0,
    };
    const D3D12_TEXTURE_COPY_LOCATION src_loc = {
        .pResource = upload_buf->buf,
        .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
        .PlacedFootprint = img->footprint,
    };
    ID3D12GraphicsCommandList_CopyTextureRegion(cmd, &dst_loc, 0, 0, 0, &src_loc, NULL);

    const D3D12_RESOURCE_BARRIER barrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Transition = {
            .pResource = img->img,
            .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            .StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
            .StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        },
    };
    ID3D12GraphicsCommandList_ResourceBarrier(cmd, 1, &barrier);

    d3d12_end_cmd(d3d12, cmd);
    d3d12_wait(d3d12);

    d3d12_destroy_buffer(upload_buf);

    return img;
}

static inline struct d3d12_pipeline *
d3d12_create_pipeline(struct d3d12 *d3d12)
{
    struct d3d12_pipeline *pipeline = (struct d3d12_pipeline *)calloc(1, sizeof(*pipeline));
    if (!pipeline)
        d3d12_die("failed to allocate pipeline");

    pipeline->desc = (D3D12_GRAPHICS_PIPELINE_STATE_DESC){
        .BlendState = {
            .RenderTarget = {
                [0] = {
                    .BlendEnable = FALSE,
                    .LogicOpEnable = FALSE,
                    .SrcBlend = D3D12_BLEND_ONE,
                    .DestBlend = D3D12_BLEND_ZERO,
                    .BlendOp = D3D12_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D12_BLEND_ONE,
                    .DestBlendAlpha = D3D12_BLEND_ZERO,
                    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                    .LogicOp = D3D12_LOGIC_OP_NOOP,
                    .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
                },
            },
        },
        .SampleMask = UINT32_MAX,
        .RasterizerState = {
            .FillMode = D3D12_FILL_MODE_SOLID,
            .CullMode = D3D12_CULL_MODE_NONE,
            .FrontCounterClockwise = FALSE,
            .DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
            .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
            .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
            .DepthClipEnable = TRUE,
        },
        .DepthStencilState = {
            .DepthEnable = FALSE,
            .StencilEnable = FALSE,
        },
	.InputLayout = {
	    .pInputElementDescs = pipeline->input_elements,
	},
        .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .NumRenderTargets = 1,
        .SampleDesc = {
            .Count = 1,
            .Quality = 0,
        },
    };

    return pipeline;
}

static inline void
d3d12_add_pipeline_root_signature(struct d3d12 *d3d12,
                                  struct d3d12_pipeline *pipeline,
                                  const D3D12_ROOT_SIGNATURE_DESC *desc)
{
    ID3DBlob *blob;

    d3d12->result =
        d3d12->SerializeRootSignature(desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, NULL);
    d3d12_check(d3d12, "SerializeRootSignature");

    d3d12->result = ID3D12Device_CreateRootSignature(
        d3d12->dev, 0, ID3D10Blob_GetBufferPointer(blob), ID3D10Blob_GetBufferSize(blob),
        &IID_ID3D12RootSignature, (void **)&pipeline->root_signature);
    d3d12_check(d3d12, "CreateRootSignature");

    ID3D10Blob_Release(blob);

    pipeline->desc.pRootSignature = pipeline->root_signature;
}

static inline void
d3d12_compile_pipeline(struct d3d12 *d3d12, struct d3d12_pipeline *pipeline)
{
    d3d12->result = ID3D12Device_CreateGraphicsPipelineState(
        d3d12->dev, &pipeline->desc, &IID_ID3D12PipelineState, (void **)&pipeline->pipeline);
    d3d12_check(d3d12, "CreateGraphicsPipelineState");
}

static inline void
d3d12_destroy_pipeline(struct d3d12_pipeline *pipeline)
{
    ID3D12PipelineState_Release(pipeline->pipeline);
    ID3D12RootSignature_Release(pipeline->root_signature);

    free(pipeline);
}

static inline void
d3d12_dump_image(struct d3d12 *d3d12,
                 const struct d3d12_image *img,
                 D3D12_RESOURCE_STATES current_state,
                 const char *filename)
{
    struct d3d12_buffer *rb =
        d3d12_create_buffer(d3d12, img->total_bytes, D3D12_HEAP_TYPE_READBACK,
                            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE);

    ID3D12GraphicsCommandList *cmd = d3d12_begin_cmd(d3d12);

    if (current_state != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        const D3D12_RESOURCE_BARRIER barrier = {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Transition = {
                .pResource = img->img,
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = current_state,
                .StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE,
            },
        };
        ID3D12GraphicsCommandList_ResourceBarrier(cmd, 1, &barrier);
    }

    const D3D12_TEXTURE_COPY_LOCATION src_loc = {
        .pResource = img->img,
        .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        .SubresourceIndex = 0,
    };
    const D3D12_TEXTURE_COPY_LOCATION dst_loc = {
        .pResource = rb->buf,
        .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
        .PlacedFootprint = img->footprint,
    };
    ID3D12GraphicsCommandList_CopyTextureRegion(cmd, &dst_loc, 0, 0, 0, &src_loc, NULL);

    d3d12_end_cmd(d3d12, cmd);
    d3d12_wait(d3d12);

    u_write_ppm(filename, (const uint8_t *)rb->map + img->footprint.Offset, img->width,
                img->height, img->footprint.Footprint.RowPitch);

    d3d12_destroy_buffer(rb);
}

#endif /* D3D12UTIL_H */
