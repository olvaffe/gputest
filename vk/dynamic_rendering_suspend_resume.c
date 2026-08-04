/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static const uint32_t dynamic_rendering_suspend_resume_test_vs[] = {
#include "dynamic_rendering_suspend_resume_test.vert.inc"
};

static const uint32_t dynamic_rendering_suspend_resume_test_fs[] = {
#include "dynamic_rendering_suspend_resume_test.frag.inc"
};

struct dynamic_rendering_suspend_resume_test {
    VkFormat color_format;
    uint32_t width;
    uint32_t height;

    struct vk vk;

    struct vk_image *rt;

    struct vk_pipeline *pipeline;
};

static void
dynamic_rendering_suspend_resume_test_init_pipeline(
    struct dynamic_rendering_suspend_resume_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT,
                           dynamic_rendering_suspend_resume_test_vs,
                           sizeof(dynamic_rendering_suspend_resume_test_vs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT,
                           dynamic_rendering_suspend_resume_test_fs,
                           sizeof(dynamic_rendering_suspend_resume_test_fs));

    test->pipeline->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    vk_set_pipeline_viewport(vk, test->pipeline, test->width, test->height);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_FILL, false);

    vk_set_pipeline_sample_count(vk, test->pipeline, VK_SAMPLE_COUNT_1_BIT);

    vk_set_pipeline_push_const(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT,
                               sizeof(float[4]));

    test->pipeline->rendering_info = (VkPipelineRenderingCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &test->color_format,
    };

    vk_compile_pipeline(vk, test->pipeline);
}

static void
dynamic_rendering_suspend_resume_test_init_framebuffer(
    struct dynamic_rendering_suspend_resume_test *test)
{
    struct vk *vk = &test->vk;

    test->rt =
        vk_create_image(vk, test->color_format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    vk_create_image_render_view(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT);
}

static void
dynamic_rendering_suspend_resume_test_init(struct dynamic_rendering_suspend_resume_test *test)
{
    struct vk *vk = &test->vk;
    vk_init(vk, NULL);

    dynamic_rendering_suspend_resume_test_init_framebuffer(test);
    dynamic_rendering_suspend_resume_test_init_pipeline(test);
}

static void
dynamic_rendering_suspend_resume_test_cleanup(struct dynamic_rendering_suspend_resume_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_pipeline(vk, test->pipeline);

    vk_destroy_image(vk, test->rt);

    vk_cleanup(vk);
}

static void
dynamic_rendering_suspend_resume_test_draw_begin_rendering(
    struct dynamic_rendering_suspend_resume_test *test,
    VkCommandBuffer cmd,
    VkRenderingFlags flags)
{
    struct vk *vk = &test->vk;

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
        .flags = flags,
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
}

static void
dynamic_rendering_suspend_resume_test_draw_triangle_1(
    struct dynamic_rendering_suspend_resume_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier2 before_barrier = {
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

    const VkDependencyInfo dep_info1 = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &before_barrier,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info1);

    dynamic_rendering_suspend_resume_test_draw_begin_rendering(test, cmd,
                                                               VK_RENDERING_SUSPENDING_BIT);
    vk_bind_pipeline(vk, test->pipeline, cmd);

    const float red[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    const VkPushConstantsInfo red_push_info = {
        .sType = VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO,
        .layout = test->pipeline->pipeline_layout,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .size = sizeof(red),
        .pValues = red,
    };
    vk->CmdPushConstants2(cmd, &red_push_info);
    vk->CmdDraw(cmd, 3, 1, 0, 0);
    vk->CmdEndRendering(cmd);
}

static void
dynamic_rendering_suspend_resume_test_draw_triangle_2(
    struct dynamic_rendering_suspend_resume_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    dynamic_rendering_suspend_resume_test_draw_begin_rendering(
        test, cmd, VK_RENDERING_SUSPENDING_BIT | VK_RENDERING_RESUMING_BIT);
    vk_bind_pipeline(vk, test->pipeline, cmd);

    const float green[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    const VkPushConstantsInfo green_push_info = {
        .sType = VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO,
        .layout = test->pipeline->pipeline_layout,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .size = sizeof(green),
        .pValues = green,
    };
    vk->CmdPushConstants2(cmd, &green_push_info);
    vk->CmdDraw(cmd, 3, 1, 1, 0);
    vk->CmdEndRendering(cmd);
}

static void
dynamic_rendering_suspend_resume_test_draw_triangle_3(
    struct dynamic_rendering_suspend_resume_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier2 after_barrier = {
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

    dynamic_rendering_suspend_resume_test_draw_begin_rendering(test, cmd,
                                                               VK_RENDERING_RESUMING_BIT);
    vk_bind_pipeline(vk, test->pipeline, cmd);

    const float blue[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    const VkPushConstantsInfo blue_push_info = {
        .sType = VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO,
        .layout = test->pipeline->pipeline_layout,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .size = sizeof(blue),
        .pValues = blue,
    };
    vk->CmdPushConstants2(cmd, &blue_push_info);
    vk->CmdDraw(cmd, 3, 1, 2, 0);
    vk->CmdEndRendering(cmd);

    const VkDependencyInfo dep_info2 = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &after_barrier,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info2);
}

static void
dynamic_rendering_suspend_resume_test_draw(struct dynamic_rendering_suspend_resume_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmds[3];
    const VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vk->cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = ARRAY_SIZE(cmds),
    };
    vk->result = vk->AllocateCommandBuffers(vk->dev, &alloc_info, cmds);
    vk_check(vk, "failed to allocate command buffers");

    for (uint32_t i = 0; i < ARRAY_SIZE(cmds); i++) {
        const VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };
        vk->result = vk->BeginCommandBuffer(cmds[i], &begin_info);
        vk_check(vk, "failed to begin command buffer");
    }

    dynamic_rendering_suspend_resume_test_draw_triangle_1(test, cmds[0]);
    dynamic_rendering_suspend_resume_test_draw_triangle_2(test, cmds[1]);
    dynamic_rendering_suspend_resume_test_draw_triangle_3(test, cmds[2]);

    for (uint32_t i = 0; i < ARRAY_SIZE(cmds); i++) {
        vk->result = vk->EndCommandBuffer(cmds[i]);
        vk_check(vk, "failed to end command buffer");
    }

    VkCommandBufferSubmitInfo cmd_infos[ARRAY_SIZE(cmds)];
    for (uint32_t i = 0; i < ARRAY_SIZE(cmds); i++) {
        cmd_infos[i] = (VkCommandBufferSubmitInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cmds[i],
        };
    }
    const VkSubmitInfo2 submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = ARRAY_SIZE(cmd_infos),
        .pCommandBufferInfos = cmd_infos,
    };
    vk->result = vk->QueueSubmit2(vk->queue, 1, &submit_info, VK_NULL_HANDLE);
    vk_check(vk, "failed to submit command buffer");

    vk_wait(vk);

    vk_dump_image(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT, "rt.ppm");
}

int
main(void)
{
    struct dynamic_rendering_suspend_resume_test test = {
        .color_format = VK_FORMAT_B8G8R8A8_UNORM,
        .width = 300,
        .height = 300,
    };

    dynamic_rendering_suspend_resume_test_init(&test);
    dynamic_rendering_suspend_resume_test_draw(&test);
    dynamic_rendering_suspend_resume_test_cleanup(&test);

    return 0;
}
