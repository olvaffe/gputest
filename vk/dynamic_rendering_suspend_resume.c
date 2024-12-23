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

    vk_set_pipeline_topology(vk, test->pipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

    vk_set_pipeline_viewport(vk, test->pipeline, test->width, test->height);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_FILL);

    vk_set_pipeline_sample_count(vk, test->pipeline, VK_SAMPLE_COUNT_1_BIT);

    vk_set_pipeline_push_const(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT,
                               sizeof(float[4]));

    vk_setup_pipeline(vk, test->pipeline, NULL);
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
    const struct vk_init_params params = {
        .api_version = VK_API_VERSION_1_3,
        .enable_all_features = true,
    };

    vk_init(vk, &params);

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
    const VkImageMemoryBarrier before_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = test->rt->img,
        .subresourceRange = subres_range,
    };

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1,
                           &before_barrier);

    dynamic_rendering_suspend_resume_test_draw_begin_rendering(test, cmd,
                                                               VK_RENDERING_SUSPENDING_BIT);
    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, test->pipeline->pipeline);

    const float red[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    vk->CmdPushConstants(cmd, test->pipeline->pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(red), red);
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
    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, test->pipeline->pipeline);

    const float green[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    vk->CmdPushConstants(cmd, test->pipeline->pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(green), green);
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
    const VkImageMemoryBarrier after_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = test->rt->img,
        .subresourceRange = subres_range,
    };

    dynamic_rendering_suspend_resume_test_draw_begin_rendering(test, cmd,
                                                               VK_RENDERING_RESUMING_BIT);
    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, test->pipeline->pipeline);

    const float blue[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    vk->CmdPushConstants(cmd, test->pipeline->pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(blue), blue);
    vk->CmdDraw(cmd, 3, 1, 2, 0);
    vk->CmdEndRendering(cmd);

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 0, NULL, 1, &after_barrier);
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

    const VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = ARRAY_SIZE(cmds),
        .pCommandBuffers = cmds,
    };
    vk->result = vk->QueueSubmit(vk->queue, 1, &submit_info, VK_NULL_HANDLE);
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
