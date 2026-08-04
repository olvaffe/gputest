/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

/* This test draws an RGB triangle to a tiled MSAA color image, resolves it to
 * a linear image, and dumps the linear image to a file.
 *
 * A render pass is used to clear, draw, and resolve the MSAA image.
 */

#include "vkutil.h"

static const uint32_t msaa_test_vs[] = {
#include "msaa_test.vert.inc"
};

static const uint32_t msaa_test_fs[] = {
#include "msaa_test.frag.inc"
};

static const float msaa_test_vertices[3][5] = {
    {
        -1.0f, /* x */
        -1.0f, /* y */
        1.0f,  /* r */
        0.0f,  /* g */
        0.0f,  /* b */
    },
    {
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        0.0f,
    },
    {
        1.0f,
        -1.0f,
        0.0f,
        0.0f,
        1.0f,
    },
};

struct msaa_test {
    VkFormat color_format;
    uint32_t width;
    uint32_t height;

    struct vk vk;
    struct vk_buffer *vb;

    struct vk_image *rt;
    struct vk_image *resolved;
    VkRenderingAttachmentInfo color_att;
    VkRenderingInfo rendering_info;

    struct vk_pipeline *pipeline;
};

static void
msaa_test_init_pipeline(struct msaa_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, msaa_test_vs,
                           sizeof(msaa_test_vs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, msaa_test_fs,
                           sizeof(msaa_test_fs));

    const uint32_t comp_counts[2] = { 2, 3 };
    vk_set_pipeline_vertices(vk, test->pipeline, comp_counts, ARRAY_SIZE(comp_counts));
    test->pipeline->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    vk_set_pipeline_viewport(vk, test->pipeline, test->width, test->height);

    test->pipeline->sample_count = VK_SAMPLE_COUNT_4_BIT;

    test->pipeline->color_att_format = test->color_format;
    vk_compile_pipeline(vk, test->pipeline);
}

static void
msaa_test_init_framebuffer(struct msaa_test *test)
{
    struct vk *vk = &test->vk;

    test->rt =
        vk_create_image(vk, test->color_format, test->width, test->height, VK_SAMPLE_COUNT_4_BIT,
                        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    vk_create_image_render_view(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT);

    test->resolved =
        vk_create_image(vk, test->color_format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    vk_create_image_render_view(vk, test->resolved, VK_IMAGE_ASPECT_COLOR_BIT);

    test->color_att = (VkRenderingAttachmentInfo){
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = test->rt->render_view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT,
        .resolveImageView = test->resolved->render_view,
        .resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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
msaa_test_init_vb(struct msaa_test *test)
{
    struct vk *vk = &test->vk;

    test->vb =
        vk_create_buffer(vk, 0, sizeof(msaa_test_vertices), VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT);
    memcpy(test->vb->mem_ptr, msaa_test_vertices, sizeof(msaa_test_vertices));
}

static void
msaa_test_init(struct msaa_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);
    msaa_test_init_vb(test);

    msaa_test_init_framebuffer(test);
    msaa_test_init_pipeline(test);
}

static void
msaa_test_cleanup(struct msaa_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_pipeline(vk, test->pipeline);

    vk_destroy_image(vk, test->rt);
    vk_destroy_image(vk, test->resolved);

    vk_destroy_buffer(vk, test->vb);

    vk_cleanup(vk);
}

static void
msaa_test_draw_pre(struct msaa_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageMemoryBarrier2 barriers[] = {
        [0] = {
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
        },
        [1] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .image = test->resolved->img,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        },
    };

    const VkDependencyInfo dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 2,
        .pImageMemoryBarriers = barriers,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info);
}

static void
msaa_test_draw_post(struct msaa_test *test, VkCommandBuffer cmd)
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
        .image = test->resolved->img,
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
msaa_test_draw_triangle(struct msaa_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    vk->CmdBeginRendering(cmd, &test->rendering_info);

    vk->CmdBindVertexBuffers2(cmd, 0, 1, &test->vb->buf, &(VkDeviceSize){ 0 }, NULL, NULL);
    vk_bind_pipeline(vk, test->pipeline, cmd);

    vk->CmdDraw(cmd, 3, 1, 0, 0);

    vk->CmdEndRendering(cmd);
}

static void
msaa_test_draw(struct msaa_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    msaa_test_draw_pre(test, cmd);
    msaa_test_draw_triangle(test, cmd);
    msaa_test_draw_post(test, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    vk_dump_image(vk, test->resolved, VK_IMAGE_ASPECT_COLOR_BIT, "rt.ppm");
}

int
main(void)
{
    struct msaa_test test = {
        .color_format = VK_FORMAT_B8G8R8A8_UNORM,
        .width = 300,
        .height = 300,
    };

    msaa_test_init(&test);
    msaa_test_draw(&test);
    msaa_test_cleanup(&test);

    return 0;
}
