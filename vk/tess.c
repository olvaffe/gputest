/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

/* This test draws tessellated triangle to a linear color image and dumps it
 * to a file.
 */

#include "vkutil.h"

static const uint32_t tess_test_vs[] = {
#include "tess_test.vert.inc"
};

static const uint32_t tess_test_tcs[] = {
#include "tess_test.tesc.inc"
};

static const uint32_t tess_test_tes[] = {
#include "tess_test.tese.inc"
};

static const uint32_t tess_test_fs[] = {
#include "tess_test.frag.inc"
};

static const float tess_test_vertices[3][5] = {
    {
        -0.9f, /* x */
        -0.9f, /* y */
        1.0f,  /* r */
        0.0f,  /* g */
        0.0f,  /* b */
    },
    {
        0.0f,
        0.9f,
        0.0f,
        1.0f,
        0.0f,
    },
    {
        0.9f,
        -0.9f,
        0.0f,
        0.0f,
        1.0f,
    },
};

struct tess_test {
    VkFormat color_format;
    uint32_t width;
    uint32_t height;

    struct vk vk;
    struct vk_buffer *vb;

    struct vk_image *rt;
    VkRenderingAttachmentInfo color_att;
    VkRenderingInfo rendering_info;

    struct vk_pipeline *pipeline;
};

static void
tess_test_init_pipeline(struct tess_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, tess_test_vs,
                           sizeof(tess_test_vs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                           tess_test_tcs, sizeof(tess_test_tcs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                           tess_test_tes, sizeof(tess_test_tes));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, tess_test_fs,
                           sizeof(tess_test_fs));

    const uint32_t comp_counts[2] = { 2, 3 };
    vk_set_pipeline_vertices(vk, test->pipeline, comp_counts, ARRAY_SIZE(comp_counts));
    test->pipeline->topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;

    vk_set_pipeline_viewport(vk, test->pipeline, test->width, test->height);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_LINE, false);
    test->pipeline->patch_control_points = 3;
    test->pipeline->rendering_info = (VkPipelineRenderingCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &test->color_format,
    };
    vk_compile_pipeline(vk, test->pipeline);
}

static void
tess_test_init_framebuffer(struct tess_test *test)
{
    struct vk *vk = &test->vk;

    test->rt =
        vk_create_image(vk, test->color_format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    vk_create_image_render_view(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT);

    test->color_att = (VkRenderingAttachmentInfo){
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = test->rt->render_view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {
            .color = {
                .float32 = { 0.2f, 0.2f, 0.2f, 1.0f },
            },
        },
    };
    test->rendering_info = (VkRenderingInfo){
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .extent = {
                .width = test->width,
                .height = test->height,
            },
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &test->color_att,
    };
}

static void
tess_test_init_vb(struct tess_test *test)
{
    struct vk *vk = &test->vk;

    test->vb =
        vk_create_buffer(vk, 0, sizeof(tess_test_vertices), VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT);
    memcpy(test->vb->mem_ptr, tess_test_vertices, sizeof(tess_test_vertices));
}

static void
tess_test_init(struct tess_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);

    if (!vk->features.features.tessellationShader)
        vk_die("no tessellation shader support");
    if (!vk->features.features.fillModeNonSolid)
        vk_die("no non-solid fill mode support");

    tess_test_init_vb(test);

    tess_test_init_framebuffer(test);
    tess_test_init_pipeline(test);
}

static void
tess_test_cleanup(struct tess_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_pipeline(vk, test->pipeline);

    vk_destroy_image(vk, test->rt);

    vk_destroy_buffer(vk, test->vb);

    vk_cleanup(vk);
}

static void
tess_test_draw_pre(struct tess_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = test->rt->img,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    const VkDependencyInfo dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info);
}

static void
tess_test_draw_post(struct tess_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = test->rt->img,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    const VkDependencyInfo dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info);
}

static void
tess_test_draw_triangle(struct tess_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    vk->CmdBeginRendering(cmd, &test->rendering_info);

    vk->CmdBindVertexBuffers2(cmd, 0, 1, &test->vb->buf, &(VkDeviceSize){ 0 }, NULL, NULL);
    vk_bind_pipeline(vk, test->pipeline, cmd);

    vk->CmdDraw(cmd, 3, 1, 0, 0);

    vk->CmdEndRendering(cmd);
}

static void
tess_test_draw(struct tess_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    tess_test_draw_pre(test, cmd);
    tess_test_draw_triangle(test, cmd);
    tess_test_draw_post(test, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    vk_dump_image(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT, "rt.ppm");
}

int
main(void)
{
    struct tess_test test = {
        .color_format = VK_FORMAT_B8G8R8A8_UNORM,
        .width = 300,
        .height = 300,
    };

    tess_test_init(&test);
    tess_test_draw(&test);
    tess_test_cleanup(&test);

    return 0;
}
