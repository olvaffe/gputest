/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef MTLUTIL_H
#define MTLUTIL_H

#include "util.h"

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

    NS::AutoreleasePool *pool;
    NS::Error *error;

    MTL::Device *dev;
    MTL4::Compiler *compiler;
    MTL::ResidencySet *residency_set;

    MTL4::CommandQueue *queue;
    MTL::SharedEvent *event;
    uint64_t event_val;

    struct {
        MTL4::CommandAllocator *cmd_allocs[4];
        uint64_t event_vals[4];
        uint32_t count;
        uint32_t next;

        MTL4::CommandBuffer *cmd;
    } submit;
};

struct mtl_buffer {
    uint64_t size;
    MTL::ResourceOptions options;

    MTL::Buffer *buf;
    void *map;
};

struct mtl_image {
    uint32_t width;
    uint32_t height;
    MTL::PixelFormat format;
    MTL::StorageMode storage_mode;
    MTL::TextureUsage usage;

    MTL::Texture *img;
};

struct mtl_pipeline {
    MTL::Library *lib;
    MTL4::LibraryFunctionDescriptor *vs_desc;
    MTL4::LibraryFunctionDescriptor *fs_desc;
    MTL::VertexDescriptor *vertex_desc;
    MTL4::RenderPipelineDescriptor *desc;

    MTL::RenderPipelineState *pipeline;
};

static inline void
mtl_init_dev(struct mtl *mtl)
{
    mtl->dev = MTL::CreateSystemDefaultDevice();
    if (!mtl->dev)
        mtl_die("failed to create device");

    if (!mtl->dev->supportsFamily(MTL::GPUFamilyMetal4))
        mtl_die("Metal 4 required");

    MTL4::CompilerDescriptor *comp_desc = MTL4::CompilerDescriptor::alloc()->init();
    mtl->compiler = mtl->dev->newCompiler(comp_desc, &mtl->error);
    comp_desc->release();
    mtl_check(mtl, "newCompiler");
}

static inline void
mtl_init_queue(struct mtl *mtl)
{
    mtl->queue = mtl->dev->newMTL4CommandQueue();
    if (!mtl->queue)
        mtl_die("failed to create command queue");

    mtl->event = mtl->dev->newSharedEvent();
    if (!mtl->event)
        mtl_die("failed to create shared event");

    MTL::ResidencySetDescriptor *res_desc = MTL::ResidencySetDescriptor::alloc()->init();
    mtl->residency_set = mtl->dev->newResidencySet(res_desc, &mtl->error);
    res_desc->release();
    mtl_check(mtl, "newResidencySet");

    mtl->queue->addResidencySet(mtl->residency_set);
}

static inline void
mtl_init_cmd(struct mtl *mtl)
{
    mtl->submit.count = ARRAY_SIZE(mtl->submit.cmd_allocs);
    for (uint32_t i = 0; i < mtl->submit.count; i++) {
        mtl->submit.cmd_allocs[i] = mtl->dev->newCommandAllocator();
        if (!mtl->submit.cmd_allocs[i])
            mtl_die("failed to create command allocator");

        mtl->submit.event_vals[i] = mtl->event_val;
    }
}

static inline void
mtl_init(struct mtl *mtl, const struct mtl_init_params *params)
{
    memset(mtl, 0, sizeof(*mtl));

    if (params)
        mtl->params = *params;

    mtl->pool = NS::AutoreleasePool::alloc()->init();

    mtl_init_dev(mtl);
    mtl_init_queue(mtl);
    mtl_init_cmd(mtl);
}

static inline uint64_t
mtl_signal_event(struct mtl *mtl)
{
    const uint64_t v = ++mtl->event_val;
    mtl->queue->signalEvent(mtl->event, v);
    return v;
}

static inline void
mtl_wait_event_val(struct mtl *mtl, uint64_t val)
{
    if (!mtl->event->waitUntilSignaledValue(val, UINT64_MAX))
        mtl_die("waitUntilSignaledValue failed");
}

static inline void
mtl_wait(struct mtl *mtl)
{
    mtl_wait_event_val(mtl, mtl->event_val);
}

static inline void
mtl_cleanup(struct mtl *mtl)
{
    mtl_wait(mtl);

    if (mtl->submit.cmd)
        mtl->submit.cmd->release();

    for (uint32_t i = 0; i < mtl->submit.count; i++)
        mtl->submit.cmd_allocs[i]->release();

    mtl->residency_set->release();
    mtl->event->release();
    mtl->queue->release();

    mtl->compiler->release();
    mtl->dev->release();

    mtl->pool->release();
}

static inline MTL4::CommandBuffer *
mtl_begin_cmd(struct mtl *mtl)
{
    const uint32_t next = mtl->submit.next;

    if (!mtl->submit.cmd) {
        mtl->submit.cmd = mtl->dev->newCommandBuffer();
        if (!mtl->submit.cmd)
            mtl_die("failed to create command buffer");
    } else {
        mtl_wait_event_val(mtl, mtl->submit.event_vals[next]);
        mtl->submit.cmd_allocs[next]->reset();
    }

    mtl->submit.cmd->beginCommandBuffer(mtl->submit.cmd_allocs[next]);

    return mtl->submit.cmd;
}

static inline void
mtl_end_cmd(struct mtl *mtl, MTL4::CommandBuffer *cmd)
{
    cmd->endCommandBuffer();

    mtl->residency_set->commit();
    mtl->queue->commit(&cmd, 1);

    const uint32_t next = mtl->submit.next;
    mtl->submit.event_vals[next] = mtl_signal_event(mtl);
    mtl->submit.next = (next + 1) % mtl->submit.count;
}

static inline struct mtl_buffer *
mtl_create_buffer(struct mtl *mtl, uint64_t size, MTL::ResourceOptions options)
{
    struct mtl_buffer *buf = (struct mtl_buffer *)calloc(1, sizeof(*buf));
    if (!buf)
        mtl_die("failed to allocate buffer");

    buf->size = size;
    buf->options = options;

    buf->buf = mtl->dev->newBuffer(size, options);
    if (!buf->buf)
        mtl_die("failed to create buffer");

    if (buf->buf->storageMode() == MTL::StorageModeShared)
        buf->map = buf->buf->contents();

    mtl->residency_set->addAllocation(buf->buf);

    return buf;
}

static inline void
mtl_destroy_buffer(struct mtl *mtl, struct mtl_buffer *buf)
{
    mtl->residency_set->removeAllocation(buf->buf);

    buf->buf->release();
    free(buf);
}

static inline struct mtl_image *
mtl_create_image(struct mtl *mtl,
                 uint32_t width,
                 uint32_t height,
                 MTL::PixelFormat format,
                 MTL::StorageMode storage_mode,
                 MTL::TextureUsage usage)
{
    struct mtl_image *img = (struct mtl_image *)calloc(1, sizeof(*img));
    if (!img)
        mtl_die("failed to allocate image");

    img->width = width;
    img->height = height;
    img->format = format;
    img->storage_mode = storage_mode;
    img->usage = usage;

    MTL::TextureDescriptor *desc = MTL::TextureDescriptor::alloc()->init();
    desc->setTextureType(MTL::TextureType2D);
    desc->setPixelFormat(format);
    desc->setWidth(width);
    desc->setHeight(height);
    desc->setStorageMode(storage_mode);
    desc->setUsage(usage);

    img->img = mtl->dev->newTexture(desc);
    desc->release();
    if (!img->img)
        mtl_die("failed to create texture");

    mtl->residency_set->addAllocation(img->img);

    return img;
}

static inline void
mtl_destroy_image(struct mtl *mtl, struct mtl_image *img)
{
    mtl->residency_set->removeAllocation(img->img);

    img->img->release();
    free(img);
}

static inline struct mtl_pipeline *
mtl_create_pipeline(struct mtl *mtl)
{
    struct mtl_pipeline *pipeline = (struct mtl_pipeline *)calloc(1, sizeof(*pipeline));
    if (!pipeline)
        mtl_die("failed to allocate pipeline");

    pipeline->vertex_desc = MTL::VertexDescriptor::alloc()->init();

    pipeline->desc = MTL4::RenderPipelineDescriptor::alloc()->init();
    pipeline->desc->setInputPrimitiveTopology(MTL::PrimitiveTopologyClassTriangle);

    return pipeline;
}

static inline void
mtl_add_pipeline_shader(struct mtl *mtl,
                        struct mtl_pipeline *pipeline,
                        const char *msl,
                        const char *vs_name,
                        const char *fs_name)
{
    NS::String *src = NS::String::alloc()->init(msl, NS::UTF8StringEncoding);
    MTL4::LibraryDescriptor *lib_desc = MTL4::LibraryDescriptor::alloc()->init();
    lib_desc->setSource(src);

    pipeline->lib = mtl->compiler->newLibrary(lib_desc, &mtl->error);
    lib_desc->release();
    src->release();
    mtl_check(mtl, "newLibrary");

    if (vs_name) {
        NS::String *name = NS::String::alloc()->init(vs_name, NS::UTF8StringEncoding);
        pipeline->vs_desc = MTL4::LibraryFunctionDescriptor::alloc()->init();
        pipeline->vs_desc->setLibrary(pipeline->lib);
        pipeline->vs_desc->setName(name);
        name->release();

        pipeline->desc->setVertexFunctionDescriptor(pipeline->vs_desc);
    }

    if (fs_name) {
        NS::String *name = NS::String::alloc()->init(fs_name, NS::UTF8StringEncoding);
        pipeline->fs_desc = MTL4::LibraryFunctionDescriptor::alloc()->init();
        pipeline->fs_desc->setLibrary(pipeline->lib);
        pipeline->fs_desc->setName(name);
        name->release();

        pipeline->desc->setFragmentFunctionDescriptor(pipeline->fs_desc);
    }
}

static inline void
mtl_compile_pipeline(struct mtl *mtl, struct mtl_pipeline *pipeline)
{
    pipeline->desc->setVertexDescriptor(pipeline->vertex_desc);

    pipeline->pipeline =
        mtl->compiler->newRenderPipelineState(pipeline->desc, nullptr, &mtl->error);
    mtl_check(mtl, "newRenderPipelineState");
}

static inline void
mtl_destroy_pipeline(struct mtl *mtl, struct mtl_pipeline *pipeline)
{
    if (pipeline->pipeline)
        pipeline->pipeline->release();
    pipeline->desc->release();
    pipeline->vertex_desc->release();
    if (pipeline->fs_desc)
        pipeline->fs_desc->release();
    if (pipeline->vs_desc)
        pipeline->vs_desc->release();
    if (pipeline->lib)
        pipeline->lib->release();

    free(pipeline);
}

static inline void
mtl_dump_image(struct mtl *mtl, const struct mtl_image *img, const char *filename)
{
    const uint64_t stride = img->width * 4;
    const uint64_t size = stride * img->height;
    struct mtl_buffer *buf = mtl_create_buffer(mtl, size, MTL::ResourceStorageModeShared);

    MTL4::CommandBuffer *cmd = mtl_begin_cmd(mtl);

    MTL4::ComputeCommandEncoder *enc = cmd->computeCommandEncoder();
    enc->copyFromTexture(img->img, 0, 0, MTL::Origin::Make(0, 0, 0),
                         MTL::Size::Make(img->width, img->height, 1), buf->buf, 0, stride, size);
    enc->endEncoding();

    mtl_end_cmd(mtl, cmd);
    mtl_wait(mtl);

    u_write_ppm(filename, buf->map, img->width, img->height, stride);

    mtl_destroy_buffer(mtl, buf);
}

#endif /* MTLUTIL_H */
