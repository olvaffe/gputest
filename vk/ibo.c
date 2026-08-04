/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static const uint32_t ibo_test_vs[] = {
#include "ibo_test.vert.inc"
};

static const uint32_t ibo_test_fs[] = {
#include "ibo_test.frag.inc"
};

struct ibo_test {
    VkFormat color_format;
    VkFormat ibo_format;
    uint32_t width;
    uint32_t height;
    uint32_t point_count;

    struct vk vk;

    struct vk_buffer *ibo;
    VkBufferView ibo_view;

    struct vk_image *rt;

    struct vk_pipeline *pipeline;
    struct vk_descriptor_set *set;
};

static void
ibo_test_init_descriptor_set(struct ibo_test *test)
{
    struct vk *vk = &test->vk;

    test->set = vk_create_descriptor_set(vk, test->pipeline->set_layouts[0]);

    const VkWriteDescriptorSet write_info = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = test->set->set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
        .pTexelBufferView = &test->ibo_view,
    };
    vk->UpdateDescriptorSets(vk->dev, 1, &write_info, 0, NULL);
}

static void
ibo_test_init_pipeline(struct ibo_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, ibo_test_vs,
                           sizeof(ibo_test_vs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, ibo_test_fs,
                           sizeof(ibo_test_fs));

    vk_add_pipeline_set_layout(vk, test->pipeline, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1,
                               VK_SHADER_STAGE_VERTEX_BIT, NULL);

    vk_set_pipeline_topology(vk, test->pipeline, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

    vk_set_pipeline_viewport(vk, test->pipeline, test->width, test->height);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_FILL, false);

    vk_set_pipeline_sample_count(vk, test->pipeline, VK_SAMPLE_COUNT_1_BIT);

    test->pipeline->rendering_info = (VkPipelineRenderingCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &test->color_format,
    };
    vk_compile_pipeline(vk, test->pipeline);
}

static void
ibo_test_init_framebuffer(struct ibo_test *test)
{
    struct vk *vk = &test->vk;

    test->rt =
        vk_create_image(vk, test->color_format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    vk_create_image_render_view(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT);
}

static void
ibo_test_init_ibo(struct ibo_test *test)
{
    struct vk *vk = &test->vk;

    const uint32_t elem_count = (test->point_count + 1) / 2;
    const VkDeviceSize buf_size = sizeof(uint32_t) * elem_count;
    test->ibo = vk_create_buffer(vk, 0, buf_size, VK_BUFFER_USAGE_2_STORAGE_TEXEL_BUFFER_BIT);

    const VkBufferViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
        .buffer = test->ibo->buf,
        .format = test->ibo_format,
        .range = buf_size,
    };
    vk->result = vk->CreateBufferView(vk->dev, &view_info, NULL, &test->ibo_view);
    vk_check(vk, "failed to create ibo view");

    const uint32_t step = 37;
    for (uint32_t i = 0; i < test->point_count; i++) {
        const uint8_t x = (i * step) % test->width;
        const uint8_t y = ((i * step) / test->width) % test->height;

        uint16_t *elems = test->ibo->mem_ptr;
        elems[i] = x | (y << 8);
    }
}

static void
ibo_test_init(struct ibo_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);

    ibo_test_init_ibo(test);
    ibo_test_init_framebuffer(test);
    ibo_test_init_pipeline(test);
    ibo_test_init_descriptor_set(test);
}

static void
ibo_test_cleanup(struct ibo_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_descriptor_set(vk, test->set);
    vk_destroy_pipeline(vk, test->pipeline);

    vk_destroy_image(vk, test->rt);

    vk->DestroyBufferView(vk->dev, test->ibo_view, NULL);
    vk_destroy_buffer(vk, test->ibo);

    vk_cleanup(vk);
}

static void
ibo_test_draw_points(struct ibo_test *test, VkCommandBuffer cmd)
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
        .dstAccessMask = VK_ACCESS_2_NONE,
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

    const VkDependencyInfo dep_info1 = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier1,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info1);

    vk->CmdBeginRendering(cmd, &rendering_info);

    vk_bind_pipeline(vk, test->pipeline, cmd);

    const VkBindDescriptorSetsInfo bind_info = {
        .sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO,
        .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
        .layout = test->pipeline->pipeline_layout,
        .descriptorSetCount = 1,
        .pDescriptorSets = &test->set->set,
    };
    vk->CmdBindDescriptorSets2(cmd, &bind_info);

    vk->CmdDraw(cmd, test->point_count, 1, 0, 0);

    vk->CmdEndRendering(cmd);

    const VkDependencyInfo dep_info2 = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier2,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info2);
}

static void
ibo_test_draw(struct ibo_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    ibo_test_draw_points(test, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    vk_dump_image(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT, "rt.ppm");
}

int
main(void)
{
    struct ibo_test test = {
        .color_format = VK_FORMAT_B8G8R8A8_UNORM,
        .ibo_format = VK_FORMAT_R32_UINT,
        .width = 256,
        .height = 256,
        .point_count = 60,
    };

    ibo_test_init(&test);
    ibo_test_draw(&test);
    ibo_test_cleanup(&test);

    return 0;
}
