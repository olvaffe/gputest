/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "mtlutil.h"

static const char tri_test_msl[] = {
#include "tri_test.metal.inc"
};

static const struct {
    float pos[2];
    float color[3];
} tri_test_vertices[] = {
    {
        .pos = { 0.0f, 1.0f },
        .color = { 1.0f, 0.0f, 0.0f },
    },
    {
        .pos = { 1.0f, -1.0f },
        .color = { 0.0f, 1.0f, 0.0f },
    },
    {
        .pos = { -1.0f, -1.0f },
        .color = { 0.0f, 0.0f, 1.0f },
    },
};

struct tri_test {
    uint32_t width;
    uint32_t height;
    MTL::PixelFormat format;
    double clear_color[4];

    struct mtl mtl;

    struct mtl_buffer *vb;
    NS::SharedPtr<MTL4::ArgumentTable> vb_table;

    struct mtl_texture *rt;

    struct mtl_pipeline *pipeline;
};

static void
tri_test_init_pipeline(struct tri_test *test)
{
    struct mtl *mtl = &test->mtl;

    test->pipeline = mtl_create_pipeline(mtl);

    mtl_set_pipeline_library(mtl, test->pipeline, tri_test_msl);
    mtl_set_pipeline_vs(mtl, test->pipeline, "vs_main");
    mtl_set_pipeline_fs(mtl, test->pipeline, "fs_main");

    mtl_add_pipeline_vertex_attr(mtl, test->pipeline, 0, MTL::VertexFormatFloat2, 0, 0);
    mtl_add_pipeline_vertex_attr(mtl, test->pipeline, 1, MTL::VertexFormatFloat3,
                                 sizeof(float) * 2, 0);
    mtl_add_pipeline_vertex_layout(mtl, test->pipeline, 0, sizeof(tri_test_vertices[0]),
                                   MTL::VertexStepFunctionPerVertex);

    test->pipeline->desc->colorAttachments()->object(0)->setPixelFormat(test->format);

    mtl_compile_pipeline(mtl, test->pipeline);
}

static void
tri_test_init_rt(struct tri_test *test)
{
    struct mtl *mtl = &test->mtl;

    test->rt = mtl_create_texture(mtl, test->width, test->height, test->format,
                                  MTL::StorageModePrivate, MTL::TextureUsageRenderTarget);
}

static void
tri_test_init_vb(struct tri_test *test)
{
    struct mtl *mtl = &test->mtl;

    test->vb = mtl_create_buffer(mtl, sizeof(tri_test_vertices), MTL::ResourceStorageModeShared);

    memcpy(test->vb->map, tri_test_vertices, sizeof(tri_test_vertices));

    NS::SharedPtr<MTL4::ArgumentTableDescriptor> table_desc =
        NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
    table_desc->setMaxBufferBindCount(1);
    test->vb_table = NS::TransferPtr(mtl->dev->newArgumentTable(table_desc.get(), &mtl->error));
    mtl_check(mtl, "newArgumentTable");

    test->vb_table->setAddress(test->vb->buf->gpuAddress(), 0);
}

static void
tri_test_init(struct tri_test *test)
{
    struct mtl *mtl = &test->mtl;

    mtl_init(mtl, NULL);

    tri_test_init_vb(test);
    tri_test_init_rt(test);
    tri_test_init_pipeline(test);
}

static void
tri_test_cleanup(struct tri_test *test)
{
    struct mtl *mtl = &test->mtl;

    mtl_destroy_pipeline(mtl, test->pipeline);

    mtl_destroy_texture(mtl, test->rt);

    test->vb_table.reset();
    mtl_destroy_buffer(mtl, test->vb);

    mtl_cleanup(mtl);
}

static void
tri_test_draw(struct tri_test *test)
{
    struct mtl *mtl = &test->mtl;
    MTL4::CommandBuffer *cmd = mtl_begin_cmd(mtl);

    NS::SharedPtr<MTL4::RenderPassDescriptor> pass_desc =
        NS::TransferPtr(MTL4::RenderPassDescriptor::alloc()->init());
    MTL::RenderPassColorAttachmentDescriptor *color_att =
        pass_desc->colorAttachments()->object(0);
    color_att->setTexture(test->rt->tex.get());
    color_att->setLoadAction(MTL::LoadActionClear);
    color_att->setStoreAction(MTL::StoreActionStore);
    color_att->setClearColor(MTL::ClearColor::Make(test->clear_color[0], test->clear_color[1],
                                                   test->clear_color[2], test->clear_color[3]));

    MTL4::RenderCommandEncoder *enc = cmd->renderCommandEncoder(pass_desc.get());

    enc->setRenderPipelineState(test->pipeline->pipeline.get());
    enc->setArgumentTable(test->vb_table.get(), MTL::RenderStageVertex);

    const MTL::Viewport vp = {
        .originX = 0.0,
        .originY = 0.0,
        .width = (double)test->width,
        .height = (double)test->height,
        .znear = 0.0,
        .zfar = 1.0,
    };
    const MTL::ScissorRect scissor = {
        .x = 0,
        .y = 0,
        .width = test->width,
        .height = test->height,
    };
    enc->setViewport(vp);
    enc->setScissorRect(scissor);

    enc->drawPrimitives(MTL::PrimitiveTypeTriangle, 0, ARRAY_SIZE(tri_test_vertices));
    enc->endEncoding();

    mtl_end_cmd(mtl, cmd);
    mtl_wait(mtl);

    mtl_dump_texture(mtl, test->rt, "rt.ppm");
}

int
main(void)
{
    struct tri_test test = {
        .width = 300,
        .height = 300,
        .format = MTL::PixelFormatRGBA8Unorm,
        .clear_color = { 0.0, 0.2, 0.4, 1.0 },
    };

    tri_test_init(&test);
    tri_test_draw(&test);
    tri_test_cleanup(&test);

    return 0;
}
