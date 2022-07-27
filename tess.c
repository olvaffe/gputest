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
    struct vk_framebuffer *fb;

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
    vk_set_pipeline_layout(vk, test->pipeline, false, false);

    const uint32_t comp_counts[2] = { 2, 3 };
    vk_set_pipeline_vertices(vk, test->pipeline, comp_counts, ARRAY_SIZE(comp_counts));

    vk_set_pipeline_topology(vk, test->pipeline, VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);
    vk_set_pipeline_tessellation(vk, test->pipeline, 3);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_LINE);

    vk_setup_pipeline(vk, test->pipeline, test->fb);
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

    test->fb = vk_create_framebuffer(vk, test->rt, NULL, NULL);
}

static void
tess_test_init_vb(struct tess_test *test)
{
    struct vk *vk = &test->vk;

    test->vb =
        vk_create_buffer(vk, sizeof(tess_test_vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    memcpy(test->vb->mem_ptr, tess_test_vertices, sizeof(tess_test_vertices));
}

static void
tess_test_init(struct tess_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk);
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
    vk_destroy_framebuffer(vk, test->fb);

    vk_destroy_buffer(vk, test->vb);

    vk_cleanup(vk);
}

static void
tess_test_draw_triangle(struct tess_test *test, VkCommandBuffer cmd)
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

    vk->CmdDraw(cmd, 3, 1, 0, 0);

    vk->CmdEndRenderPass(cmd);

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 0, NULL, 1, &barrier2);
}

static void
tess_test_draw(struct tess_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk);

    tess_test_draw_triangle(test, cmd);

    vk_end_cmd(vk);

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
