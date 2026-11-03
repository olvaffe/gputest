/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef MTLUTIL_H
#define MTLUTIL_H

#include "util.h"

#include <new>

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Metal/Metal.hpp>

#define mtl_check(mtl, msg)                                                                      \
    u_check("MTL", !(mtl)->error, "%s failed: %s", msg,                                          \
            (mtl)->error->localizedDescription()->utf8String())
#define mtl_die(format, ...) u_die("MTL", format __VA_OPT__(, ) __VA_ARGS__)
#define mtl_log(format, ...) u_log("MTL", format __VA_OPT__(, ) __VA_ARGS__)

struct mtl_init_params {
    int unused;
};

struct mtl {
    struct mtl_init_params params;

    NS::Error *error;
    NS::SharedPtr<NS::AutoreleasePool> autorelease_pool;

    NS::SharedPtr<MTL::Device> dev;
    NS::SharedPtr<MTL4::CommandQueue> queue;
    NS::SharedPtr<MTL::ResidencySet> residency_set;
    NS::SharedPtr<MTL4::Compiler> compiler;

    struct {
        NS::SharedPtr<MTL::SharedEvent> shared_event;
        uint64_t event_next;

        NS::SharedPtr<MTL4::CommandBuffer> cmd;

        NS::SharedPtr<MTL4::CommandAllocator> cmd_allocs[4];
        uint64_t event_vals[4];
        uint32_t count;
        uint32_t next;
    } submit;
};

struct mtl_buffer {
    NS::SharedPtr<MTL::Buffer> buf;
    void *map;
};

struct mtl_texture {
    NS::SharedPtr<MTL::Texture> tex;
};

struct mtl_pipeline {
    NS::SharedPtr<MTL4::RenderPipelineDescriptor> desc;

    NS::SharedPtr<MTL::Library> lib;

    NS::SharedPtr<MTL::RenderPipelineState> pipeline;
};

static inline void
mtl_init_dev(struct mtl *mtl)
{
    mtl->dev = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
    if (!mtl->dev)
        mtl_die("failed to create device");

    if (!mtl->dev->supportsFamily(MTL::GPUFamilyMetal4))
        mtl_die("Metal 4 required");
}

static inline void
mtl_init_queue(struct mtl *mtl)
{
    mtl->queue = NS::TransferPtr(mtl->dev->newMTL4CommandQueue());
    if (!mtl->queue)
        mtl_die("failed to create command queue");

    NS::SharedPtr<MTL::ResidencySetDescriptor> res_desc =
        NS::TransferPtr(MTL::ResidencySetDescriptor::alloc()->init());
    mtl->residency_set = NS::TransferPtr(mtl->dev->newResidencySet(res_desc.get(), &mtl->error));
    mtl_check(mtl, "newResidencySet");

    mtl->queue->addResidencySet(mtl->residency_set.get());
}

static inline void
mtl_init_compiler(struct mtl *mtl)
{
    NS::SharedPtr<MTL4::CompilerDescriptor> comp_desc =
        NS::TransferPtr(MTL4::CompilerDescriptor::alloc()->init());
    mtl->compiler = NS::TransferPtr(mtl->dev->newCompiler(comp_desc.get(), &mtl->error));
    mtl_check(mtl, "newCompiler");
}

static inline void
mtl_init_submit(struct mtl *mtl)
{
    mtl->submit.shared_event = NS::TransferPtr(mtl->dev->newSharedEvent());
    if (!mtl->submit.shared_event)
        mtl_die("failed to create shared event");
    mtl->submit.event_next = 1;

    mtl->submit.cmd = NS::TransferPtr(mtl->dev->newCommandBuffer());
    if (!mtl->submit.cmd)
        mtl_die("failed to create command buffer");

    mtl->submit.count = ARRAY_SIZE(mtl->submit.cmd_allocs);
    for (uint32_t i = 0; i < mtl->submit.count; i++) {
        mtl->submit.cmd_allocs[i] = NS::TransferPtr(mtl->dev->newCommandAllocator());
        if (!mtl->submit.cmd_allocs[i])
            mtl_die("failed to create command allocator");
    }
}

static inline void
mtl_init(struct mtl *mtl, const struct mtl_init_params *params)
{
    new (mtl) struct mtl();

    if (params)
        mtl->params = *params;

    mtl->autorelease_pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());

    mtl_init_dev(mtl);
    mtl_init_queue(mtl);
    mtl_init_compiler(mtl);
    mtl_init_submit(mtl);
}

static inline uint64_t
mtl_signal_event(struct mtl *mtl)
{
    const uint64_t val = mtl->submit.event_next++;
    mtl->queue->signalEvent(mtl->submit.shared_event.get(), val);
    return val;
}

static inline void
mtl_wait_event(struct mtl *mtl, uint64_t val)
{
    if (!mtl->submit.shared_event->waitUntilSignaledValue(val, UINT64_MAX))
        mtl_die("waitUntilSignaledValue failed");
}

static inline void
mtl_wait(struct mtl *mtl)
{
    mtl_wait_event(mtl, mtl->submit.event_next - 1);
}

static inline void
mtl_cleanup(struct mtl *mtl)
{
    mtl_wait(mtl);

    mtl->~mtl();
    new (mtl) struct mtl();
}

static inline MTL4::CommandBuffer *
mtl_begin_cmd(struct mtl *mtl)
{
    const NS::SharedPtr<MTL4::CommandAllocator> &cmd_alloc =
        mtl->submit.cmd_allocs[mtl->submit.next];
    const uint64_t event_val = mtl->submit.event_vals[mtl->submit.next];

    mtl_wait_event(mtl, event_val);
    cmd_alloc->reset();
    mtl->submit.cmd->beginCommandBuffer(cmd_alloc.get());

    return mtl->submit.cmd.get();
}

static inline void
mtl_end_cmd(struct mtl *mtl, MTL4::CommandBuffer *cmd)
{
    cmd->endCommandBuffer();

    mtl->residency_set->commit();
    mtl->queue->commit(&cmd, 1);

    mtl->submit.event_vals[mtl->submit.next] = mtl_signal_event(mtl);
    mtl->submit.next = (mtl->submit.next + 1) % mtl->submit.count;
}

static inline struct mtl_buffer *
mtl_create_buffer(struct mtl *mtl, uint64_t size, MTL::ResourceOptions options)
{
    struct mtl_buffer *buf = new mtl_buffer();

    buf->buf = NS::TransferPtr(mtl->dev->newBuffer(size, options));
    if (!buf->buf)
        mtl_die("failed to create buffer");

    if (buf->buf->storageMode() == MTL::StorageModeShared)
        buf->map = buf->buf->contents();

    mtl->residency_set->addAllocation(buf->buf.get());

    return buf;
}

static inline void
mtl_destroy_buffer(struct mtl *mtl, struct mtl_buffer *buf)
{
    mtl->residency_set->removeAllocation(buf->buf.get());

    delete buf;
}

static inline struct mtl_texture *
mtl_create_texture(struct mtl *mtl,
                   uint32_t width,
                   uint32_t height,
                   MTL::PixelFormat format,
                   MTL::StorageMode storage_mode,
                   MTL::TextureUsage usage)
{
    struct mtl_texture *tex = new mtl_texture();

    NS::SharedPtr<MTL::TextureDescriptor> desc =
        NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
    desc->setTextureType(MTL::TextureType2D);
    desc->setPixelFormat(format);
    desc->setWidth(width);
    desc->setHeight(height);
    desc->setStorageMode(storage_mode);
    desc->setUsage(usage);

    tex->tex = NS::TransferPtr(mtl->dev->newTexture(desc.get()));
    if (!tex->tex)
        mtl_die("failed to create texture");

    mtl->residency_set->addAllocation(tex->tex.get());

    return tex;
}

static inline void
mtl_destroy_texture(struct mtl *mtl, struct mtl_texture *tex)
{
    mtl->residency_set->removeAllocation(tex->tex.get());

    delete tex;
}

static inline struct mtl_pipeline *
mtl_create_pipeline(struct mtl *mtl)
{
    struct mtl_pipeline *pipeline = new mtl_pipeline();

    pipeline->desc = NS::TransferPtr(MTL4::RenderPipelineDescriptor::alloc()->init());
    pipeline->desc->setInputPrimitiveTopology(MTL::PrimitiveTopologyClassTriangle);

    return pipeline;
}

static inline void
mtl_set_pipeline_library(struct mtl *mtl, struct mtl_pipeline *pipeline, const char *msl)
{
    NS::SharedPtr<NS::String> src =
        NS::TransferPtr(NS::String::alloc()->init(msl, NS::UTF8StringEncoding));
    NS::SharedPtr<MTL4::LibraryDescriptor> lib_desc =
        NS::TransferPtr(MTL4::LibraryDescriptor::alloc()->init());
    lib_desc->setSource(src.get());

    pipeline->lib = NS::TransferPtr(mtl->compiler->newLibrary(lib_desc.get(), &mtl->error));
    mtl_check(mtl, "newLibrary");
}

static inline void
mtl_set_pipeline_vs(struct mtl *mtl, struct mtl_pipeline *pipeline, const char *vs_name)
{
    NS::SharedPtr<NS::String> name =
        NS::TransferPtr(NS::String::alloc()->init(vs_name, NS::UTF8StringEncoding));
    NS::SharedPtr<MTL4::LibraryFunctionDescriptor> vs_desc =
        NS::TransferPtr(MTL4::LibraryFunctionDescriptor::alloc()->init());
    vs_desc->setLibrary(pipeline->lib.get());
    vs_desc->setName(name.get());

    pipeline->desc->setVertexFunctionDescriptor(vs_desc.get());
}

static inline void
mtl_set_pipeline_fs(struct mtl *mtl, struct mtl_pipeline *pipeline, const char *fs_name)
{
    NS::SharedPtr<NS::String> name =
        NS::TransferPtr(NS::String::alloc()->init(fs_name, NS::UTF8StringEncoding));
    NS::SharedPtr<MTL4::LibraryFunctionDescriptor> fs_desc =
        NS::TransferPtr(MTL4::LibraryFunctionDescriptor::alloc()->init());
    fs_desc->setLibrary(pipeline->lib.get());
    fs_desc->setName(name.get());

    pipeline->desc->setFragmentFunctionDescriptor(fs_desc.get());
}

static inline void
mtl_add_pipeline_vertex_attr(struct mtl *mtl,
                             struct mtl_pipeline *pipeline,
                             uint32_t attr_idx,
                             MTL::VertexFormat format,
                             uint32_t offset,
                             uint32_t buf_idx)
{
    MTL::VertexAttributeDescriptor *attr =
        pipeline->desc->vertexDescriptor()->attributes()->object(attr_idx);
    attr->setFormat(format);
    attr->setOffset(offset);
    attr->setBufferIndex(buf_idx);
}

static inline void
mtl_add_pipeline_vertex_layout(struct mtl *mtl,
                               struct mtl_pipeline *pipeline,
                               uint32_t buf_idx,
                               uint32_t stride,
                               MTL::VertexStepFunction step_func)
{
    MTL::VertexBufferLayoutDescriptor *layout =
        pipeline->desc->vertexDescriptor()->layouts()->object(buf_idx);
    layout->setStride(stride);
    layout->setStepFunction(step_func);
}

static inline void
mtl_compile_pipeline(struct mtl *mtl, struct mtl_pipeline *pipeline)
{
    pipeline->pipeline = NS::TransferPtr(
        mtl->compiler->newRenderPipelineState(pipeline->desc.get(), nullptr, &mtl->error));
    mtl_check(mtl, "newRenderPipelineState");
}

static inline void
mtl_destroy_pipeline(struct mtl *mtl, struct mtl_pipeline *pipeline)
{
    delete pipeline;
}

static inline void
mtl_dump_texture(struct mtl *mtl, const struct mtl_texture *tex, const char *filename)
{
    const uint32_t width = tex->tex->width();
    const uint32_t height = tex->tex->height();
    const uint64_t stride = width * 4;
    const uint64_t size = stride * height;
    struct mtl_buffer *buf = mtl_create_buffer(mtl, size, MTL::ResourceStorageModeShared);

    MTL4::CommandBuffer *cmd = mtl_begin_cmd(mtl);

    MTL4::ComputeCommandEncoder *enc = cmd->computeCommandEncoder();
    enc->copyFromTexture(tex->tex.get(), 0, 0, MTL::Origin::Make(0, 0, 0),
                         MTL::Size::Make(width, height, 1), buf->buf.get(), 0, stride, size);
    enc->endEncoding();

    mtl_end_cmd(mtl, cmd);
    mtl_wait(mtl);

    u_write_ppm(filename, buf->map, width, height, stride);

    mtl_destroy_buffer(mtl, buf);
}

#endif /* MTLUTIL_H */
