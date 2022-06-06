/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

/* This test draws an RGB triangle to a linear color image and dumps it to a
 * file.
 *
 * It memsets the raw memory with vk_fill_image, which can be ignored because
 * it does not use VK_IMAGE_LAYOUT_PREINITIALIZED.  There is a border of
 * tri_border pixels.  A render pass is used to clear the render area and
 * draws the triangle.
 */

#include "vkutil.h"

static const uint32_t tri_test_vs[] = {
#include "tri.vert.inc"
};

static const uint32_t tri_test_fs[] = {
#include "tri.frag.inc"
};

static const float tri_vertices[3][5] = {
    {
        -1.0f, /* x */
        -1.0f, /* y */
        1.0f,  /* r */
        0.0f,  /* g */
        0.0f,  /* b */
    },
    {
        -1.0f,
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

const uint32_t tri_border = 10;

struct tri_test {
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
tri_test_init_pipeline(struct tri_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_set_pipeline_shaders(vk, test->pipeline, tri_test_vs, sizeof(tri_test_vs), tri_test_fs,
                            sizeof(tri_test_fs));
    vk_set_pipeline_layout(vk, test->pipeline, false);

    const uint32_t comp_counts[2] = { 2, 3 };
    vk_set_pipeline_vertices(vk, test->pipeline, comp_counts, ARRAY_SIZE(comp_counts));

    vk_setup_pipeline(vk, test->pipeline, test->fb);
    test->pipeline->viewport.x += (float)tri_border;
    test->pipeline->viewport.y += (float)tri_border;
    test->pipeline->viewport.width -= (float)tri_border * 2.0f;
    test->pipeline->viewport.height -= (float)tri_border * 2.0f;
    test->pipeline->scissor.offset.x += tri_border;
    test->pipeline->scissor.offset.y += tri_border;
    test->pipeline->scissor.extent.width -= tri_border * 2;
    test->pipeline->scissor.extent.height -= tri_border * 2;
    vk_compile_pipeline(vk, test->pipeline);
}

static void
tri_test_init_framebuffer(struct tri_test *test)
{
    struct vk *vk = &test->vk;

    test->rt =
        vk_create_image(vk, test->color_format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    vk_fill_image(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT, 0x11);
    vk_create_image_render_view(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT);

    test->fb = vk_create_framebuffer(vk, test->rt, NULL, NULL);
}

static void
tri_test_init_vb(struct tri_test *test)
{
    struct vk *vk = &test->vk;

    test->vb = vk_create_buffer(vk, sizeof(tri_vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    memcpy(test->vb->mem_ptr, tri_vertices, sizeof(tri_vertices));
}

static void
tri_test_init(struct tri_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk);
    tri_test_init_vb(test);

    tri_test_init_framebuffer(test);
    tri_test_init_pipeline(test);
}

static void
tri_test_cleanup(struct tri_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_pipeline(vk, test->pipeline);

    vk_destroy_image(vk, test->rt);
    vk_destroy_framebuffer(vk, test->fb);

    vk_destroy_buffer(vk, test->vb);

    vk_cleanup(vk);
}

static void
tri_test_draw_triangle(struct tri_test *test, VkCommandBuffer cmd)
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
            .offset = {
                .x = tri_border,
                .y = tri_border,
            },
            .extent = {
                .width = test->width - tri_border * 2,
                .height = test->height - tri_border * 2,
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

    vk->CmdDraw(cmd, 3, 1, 0, 0);

    vk->CmdEndRenderPass(cmd);

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 0, NULL, 1, &barrier2);
}

static void
tri_test_draw(struct tri_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk);

    tri_test_draw_triangle(test, cmd);

    vk_end_cmd(vk);

    vk_dump_image(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT, "rt.ppm");
}

int
main(void)
{
    struct tri_test test = {
        .color_format = VK_FORMAT_B8G8R8A8_UNORM,
        .width = 300,
        .height = 300,
    };

    tri_test_init(&test);
    tri_test_draw(&test);
    tri_test_cleanup(&test);

    return 0;
}
