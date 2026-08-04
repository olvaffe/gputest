/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

/* This test draws a colored triangle to a linear color image and dumps it
 * to a file.
 *
 * The color is a push const.
 */

#include "vkutil.h"

static const uint32_t push_const_test_vs[] = {
#include "push_const_test.vert.inc"
};

static const uint32_t push_const_test_fs[] = {
#include "push_const_test.frag.inc"
};

static const float push_const_test_color[] = {
    1.0f,
    1.0f,
    0.0f,
    1.0f,
};

struct push_const_test {
    VkFormat color_format;
    uint32_t width;
    uint32_t height;

    struct vk vk;
    struct vk_buffer *ubo;

    struct vk_image *rt;

    struct vk_pipeline *pipeline;
    struct vk_descriptor_set *set;
};

static void
push_const_test_init_descriptor_set(struct push_const_test *test)
{
    struct vk *vk = &test->vk;

    test->set = vk_create_descriptor_set(vk, test->pipeline->set_layouts[0]);
    vk_write_descriptor_set_buffer(vk, test->set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, test->ubo,
                                   VK_WHOLE_SIZE);
}

static void
push_const_init_pipeline(struct push_const_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, push_const_test_vs,
                           sizeof(push_const_test_vs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, push_const_test_fs,
                           sizeof(push_const_test_fs));

    vk_set_pipeline_topology(vk, test->pipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

    vk_set_pipeline_viewport(vk, test->pipeline, test->width, test->height);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_FILL, false);

    vk_add_pipeline_set_layout(vk, test->pipeline, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                               VK_SHADER_STAGE_FRAGMENT_BIT, NULL);
    vk_set_pipeline_push_const(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT,
                               sizeof(push_const_test_color));

    vk_set_pipeline_sample_count(vk, test->pipeline, VK_SAMPLE_COUNT_1_BIT);

    test->pipeline->rendering_info = (VkPipelineRenderingCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &test->color_format,
    };
    vk_compile_pipeline(vk, test->pipeline);
}

static void
push_const_init_framebuffer(struct push_const_test *test)
{
    struct vk *vk = &test->vk;

    test->rt =
        vk_create_image(vk, test->color_format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    vk_create_image_render_view(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT);
}

static void
push_const_test_init_ubo(struct push_const_test *test)
{
    struct vk *vk = &test->vk;

    const float color[4] = {
        0.0f,
        0.0f,
        1.0f,
        0.0f,
    };

    test->ubo = vk_create_buffer(vk, 0, sizeof(color), VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT);
    memcpy(test->ubo->mem_ptr, color, sizeof(color));
}

static void
push_const_init(struct push_const_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);
    push_const_test_init_ubo(test);

    push_const_init_framebuffer(test);
    push_const_init_pipeline(test);
    push_const_test_init_descriptor_set(test);
}

static void
push_const_cleanup(struct push_const_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_descriptor_set(vk, test->set);
    vk_destroy_pipeline(vk, test->pipeline);

    vk_destroy_image(vk, test->rt);

    vk_destroy_buffer(vk, test->ubo);

    vk_cleanup(vk);
}

static void
push_const_draw_triangle(struct push_const_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier2 barrier1 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = test->rt->img,
        .subresourceRange = subres_range,
    };
    const VkImageMemoryBarrier2 barrier2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = test->rt->img,
        .subresourceRange = subres_range,
    };

    const VkDependencyInfo dep_info1 = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier1,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info1);

    const VkRenderingAttachmentInfo att_info = {
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
    const VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .extent = {
                .width = test->width,
                .height = test->height,
            },
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &att_info,
    };
    vk->CmdBeginRendering(cmd, &rendering_info);

    vk_bind_pipeline(vk, test->pipeline, cmd);
    const VkPushConstantsInfo push_info = {
        .sType = VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO,
        .layout = test->pipeline->pipeline_layout,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .size = sizeof(push_const_test_color),
        .pValues = push_const_test_color,
    };
    vk->CmdPushConstants2(cmd, &push_info);

    const VkBindDescriptorSetsInfo bind_info = {
        .sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO,
        .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
        .layout = test->pipeline->pipeline_layout,
        .descriptorSetCount = 1,
        .pDescriptorSets = &test->set->set,
    };
    vk->CmdBindDescriptorSets2(cmd, &bind_info);

    vk->CmdDraw(cmd, 3, 1, 0, 0);

    vk->CmdEndRendering(cmd);

    const VkDependencyInfo dep_info2 = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier2,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info2);
}

static void
push_const_draw(struct push_const_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    push_const_draw_triangle(test, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    vk_dump_image(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT, "rt.ppm");
}

int
main(void)
{
    struct push_const_test test = {
        .color_format = VK_FORMAT_B8G8R8A8_UNORM,
        .width = 300,
        .height = 300,
    };

    push_const_init(&test);
    push_const_draw(&test);
    push_const_cleanup(&test);

    return 0;
}
