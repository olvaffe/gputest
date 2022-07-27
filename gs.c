/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

/* This test draws 3 circles of different colors/radius to a linear color
 * image and dumps it to a file.
 *
 * This test draws 3 points and uses a geometry shader to turn them into 3
 * circles.
 */

#include "vkutil.h"

static const uint32_t gs_test_vs[] = {
#include "gs_test.vert.inc"
};

static const uint32_t gs_test_gs[] = {
#include "gs_test.geom.inc"
};

static const uint32_t gs_test_fs[] = {
#include "gs_test.frag.inc"
};

static const float gs_test_vertices[3][6] = {
    {
        -0.6f, /* x */
        -0.6f, /* y */
        1.0f,  /* r */
        0.0f,  /* g */
        0.0f,  /* b */
        0.3f,  /* radius */
    },
    {
        0.0f,
        0.6f,
        0.0f,
        1.0f,
        0.0f,
        0.4f,
    },
    {
        0.6f,
        -0.6f,
        0.0f,
        0.0f,
        1.0f,
        0.2f,
    },
};

struct gs_test {
    VkFormat color_format;
    uint32_t width;
    uint32_t height;

    struct vk vk;
    struct vk_buffer *vb;

    struct vk_image *rt;
    struct vk_framebuffer *fb;

    struct vk_pipeline *pipeline;
};

static void
gs_test_init_pipeline(struct gs_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, gs_test_vs,
                           sizeof(gs_test_vs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_GEOMETRY_BIT, gs_test_gs,
                           sizeof(gs_test_gs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, gs_test_fs,
                           sizeof(gs_test_fs));
    vk_set_pipeline_layout(vk, test->pipeline, false, false);

    const uint32_t comp_counts[3] = { 2, 3, 1 };
    vk_set_pipeline_vertices(vk, test->pipeline, comp_counts, ARRAY_SIZE(comp_counts));

    vk_set_pipeline_topology(vk, test->pipeline, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_FILL);

    vk_setup_pipeline(vk, test->pipeline, test->fb);
    vk_compile_pipeline(vk, test->pipeline);
}

static void
gs_test_init_framebuffer(struct gs_test *test)
{
    struct vk *vk = &test->vk;

    test->rt =
        vk_create_image(vk, test->color_format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    vk_create_image_render_view(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT);

    test->fb = vk_create_framebuffer(vk, test->rt, NULL, NULL);
}

static void
gs_test_init_vb(struct gs_test *test)
{
    struct vk *vk = &test->vk;

    test->vb = vk_create_buffer(vk, sizeof(gs_test_vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    memcpy(test->vb->mem_ptr, gs_test_vertices, sizeof(gs_test_vertices));
}

static void
gs_test_init(struct gs_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk);
    gs_test_init_vb(test);

    gs_test_init_framebuffer(test);
    gs_test_init_pipeline(test);
}

static void
gs_test_cleanup(struct gs_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_pipeline(vk, test->pipeline);

    vk_destroy_image(vk, test->rt);
    vk_destroy_framebuffer(vk, test->fb);

    vk_destroy_buffer(vk, test->vb);

    vk_cleanup(vk);
}

static void
gs_test_draw_points(struct gs_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier barrier1 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = test->rt->img,
        .subresourceRange = subres_range,
    };
    const VkImageMemoryBarrier barrier2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = test->rt->img,
        .subresourceRange = subres_range,
    };

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1,
                           &barrier1);

    const VkRenderPassBeginInfo pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = test->fb->pass,
        .framebuffer = test->fb->fb,
        .renderArea = {
            .extent = {
                .width = test->width,
                .height = test->height,
            },
        },
        .clearValueCount = 1,
        .pClearValues = &(VkClearValue){
            .color = {
                .float32 = { 0.2f, 0.2f, 0.2f, 1.0f },
            },
        },
    };
    vk->CmdBeginRenderPass(cmd, &pass_info, VK_SUBPASS_CONTENTS_INLINE);

    vk->CmdBindVertexBuffers(cmd, 0, 1, &test->vb->buf, &(VkDeviceSize){ 0 });
    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, test->pipeline->pipeline);

    vk->CmdDraw(cmd, ARRAY_SIZE(gs_test_vertices), 1, 0, 0);

    vk->CmdEndRenderPass(cmd);

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 0, NULL, 1, &barrier2);
}

static void
gs_test_draw(struct gs_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk);

    gs_test_draw_points(test, cmd);

    vk_end_cmd(vk);

    vk_dump_image(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT, "rt.ppm");
}

int
main(void)
{
    struct gs_test test = {
        .color_format = VK_FORMAT_B8G8R8A8_UNORM,
        .width = 300,
        .height = 300,
    };

    gs_test_init(&test);
    gs_test_draw(&test);
    gs_test_cleanup(&test);

    return 0;
}
