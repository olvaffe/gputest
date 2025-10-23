/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

/* This test draws a rotated RGB triangle to a linear color image and dumps it
 * to a file.
 */

#include "vkutil.h"

static const uint32_t ubo_test_vs[] = {
#include "ubo_test.vert.inc"
};

static const uint32_t ubo_test_fs[] = {
#include "ubo_test.frag.inc"
};

static const float ubo_test_vertices[3][5] = {
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

struct ubo_test {
    VkFormat color_format;
    uint32_t width;
    uint32_t height;

    struct vk vk;
    struct vk_buffer *vb;
    struct vk_buffer *ubo;

    struct vk_image *rt;
    struct vk_framebuffer *fb;

    struct vk_pipeline *pipeline;
    struct vk_descriptor_set *set;
};

static void
ubo_test_init_descriptor_set(struct ubo_test *test)
{
    struct vk *vk = &test->vk;

    test->set = vk_create_descriptor_set(vk, test->pipeline->set_layouts[0]);
    vk_write_descriptor_set_buffer(vk, test->set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, test->ubo,
                                   VK_WHOLE_SIZE);
}

static void
ubo_test_init_pipeline(struct ubo_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, ubo_test_vs,
                           sizeof(ubo_test_vs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, ubo_test_fs,
                           sizeof(ubo_test_fs));

    vk_add_pipeline_set_layout(vk, test->pipeline, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                               VK_SHADER_STAGE_VERTEX_BIT, NULL);

    const uint32_t comp_counts[2] = { 2, 3 };
    vk_set_pipeline_vertices(vk, test->pipeline, comp_counts, ARRAY_SIZE(comp_counts));
    vk_set_pipeline_topology(vk, test->pipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

    vk_set_pipeline_viewport(vk, test->pipeline, test->fb->width, test->fb->height);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_FILL, false);

    vk_set_pipeline_sample_count(vk, test->pipeline, test->fb->samples);

    vk_setup_pipeline(vk, test->pipeline, test->fb);
    vk_compile_pipeline(vk, test->pipeline);
}

static void
ubo_test_init_framebuffer(struct ubo_test *test)
{
    struct vk *vk = &test->vk;

    test->rt =
        vk_create_image(vk, test->color_format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    vk_create_image_render_view(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT);

    test->fb = vk_create_framebuffer(vk, test->rt, NULL, NULL, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                     VK_ATTACHMENT_STORE_OP_STORE);
}

static void
ubo_test_init_ubo(struct ubo_test *test)
{
    struct vk *vk = &test->vk;

    const float radian = M_PI / 60.0f;
    const float c = cosf(radian);
    const float s = sinf(radian);
    /* note that std140 requires vec4 alignment */
    const float transform[2][4] = {
        [0] = { c, s },
        [1] = { -s, c },
    };

    test->ubo = vk_create_buffer(vk, 0, sizeof(transform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    memcpy(test->ubo->mem_ptr, transform, sizeof(transform));
}

static void
ubo_test_init_vb(struct ubo_test *test)
{
    struct vk *vk = &test->vk;

    test->vb =
        vk_create_buffer(vk, 0, sizeof(ubo_test_vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    memcpy(test->vb->mem_ptr, ubo_test_vertices, sizeof(ubo_test_vertices));
}

static void
ubo_test_init(struct ubo_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);
    ubo_test_init_vb(test);
    ubo_test_init_ubo(test);

    ubo_test_init_framebuffer(test);
    ubo_test_init_pipeline(test);
    ubo_test_init_descriptor_set(test);
}

static void
ubo_test_cleanup(struct ubo_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_descriptor_set(vk, test->set);
    vk_destroy_pipeline(vk, test->pipeline);

    vk_destroy_image(vk, test->rt);
    vk_destroy_framebuffer(vk, test->fb);

    vk_destroy_buffer(vk, test->vb);
    vk_destroy_buffer(vk, test->ubo);

    vk_cleanup(vk);
}

static void
ubo_test_draw_triangle(struct ubo_test *test, VkCommandBuffer cmd)
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

    vk->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              test->pipeline->pipeline_layout, 0, 1, &test->set->set, 0, NULL);

    vk->CmdDraw(cmd, 3, 1, 0, 0);

    vk->CmdEndRenderPass(cmd);

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 0, NULL, 1, &barrier2);
}

static void
ubo_test_draw(struct ubo_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    ubo_test_draw_triangle(test, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    vk_dump_image(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT, "rt.ppm");
}

int
main(void)
{
    struct ubo_test test = {
        .color_format = VK_FORMAT_B8G8R8A8_UNORM,
        .width = 300,
        .height = 300,
    };

    ubo_test_init(&test);
    ubo_test_draw(&test);
    ubo_test_cleanup(&test);

    return 0;
}
