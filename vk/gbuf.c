/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static const uint32_t gbuf_test_vs[] = {
#include "gbuf_test.vert.inc"
};

static const uint32_t gbuf_test_fs[] = {
#include "gbuf_test.frag.inc"
};

struct gbuf_test {
    VkFormat color_format;
    uint32_t width;
    uint32_t height;

    struct vk vk;

    struct vk_image *rt;
    struct vk_image *rt_gbuf;

    VkRenderingAttachmentInfo color_atts[2];
    VkRenderingInfo rendering_info;

    struct vk_pipeline *pipeline;
    struct vk_descriptor_set *set;
};

static void
gbuf_test_init_descriptor_set(struct gbuf_test *test)
{
    struct vk *vk = &test->vk;

    test->set = vk_create_descriptor_set(vk, test->pipeline->set_layouts[0]);

    const VkDescriptorImageInfo img_info = {
        .imageView = test->rt_gbuf->render_view,
        .imageLayout = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ,
    };
    const VkWriteDescriptorSet write_info = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = test->set->set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        .pImageInfo = &img_info,
    };

    vk->UpdateDescriptorSets(vk->dev, 1, &write_info, 0, NULL);
}

static void
gbuf_test_init_pipeline(struct gbuf_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, gbuf_test_vs,
                           sizeof(gbuf_test_vs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, gbuf_test_fs,
                           sizeof(gbuf_test_fs));

    vk_add_pipeline_set_layout(vk, test->pipeline, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1,
                               VK_SHADER_STAGE_FRAGMENT_BIT, NULL);

    test->pipeline->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    vk_set_pipeline_viewport(vk, test->pipeline, test->width, test->height);

    test->pipeline->color_formats[test->pipeline->color_count++] = test->color_format;
    test->pipeline->color_formats[test->pipeline->color_count++] = test->color_format;

    vk_compile_pipeline(vk, test->pipeline);
}

static void
gbuf_test_init_rt(struct gbuf_test *test)
{
    struct vk *vk = &test->vk;

    test->rt =
        vk_create_image(vk, test->color_format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    vk_create_image_render_view(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT);

    uint32_t mt_mask = 0;
    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        if (vk->mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
            mt_mask |= 1 << i;
    }

    const VkImageCreateInfo gbuf_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = test->color_format,
        .extent = {
            .width = test->width,
            .height = test->height,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                 (mt_mask ? VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT : 0),
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    if (!mt_mask) {
        vk_log("no VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT");
        mt_mask = ~0u;
    }

    test->rt_gbuf = vk_create_image_with_mt_mask(vk, &gbuf_info, mt_mask);
    vk_create_image_render_view(vk, test->rt_gbuf, VK_IMAGE_ASPECT_COLOR_BIT);

    test->color_atts[0] = (VkRenderingAttachmentInfo){
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = test->rt->render_view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {
            .color = {
                .float32 = { 1.0f, 0.0f, 0.0f, 1.0f },
            },
        },
    };
    test->color_atts[1] = (VkRenderingAttachmentInfo){
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = test->rt_gbuf->render_view,
        .imageLayout = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
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
        .colorAttachmentCount = 2,
        .pColorAttachments = test->color_atts,
    };
}

static void
gbuf_test_init(struct gbuf_test *test)
{
    struct vk *vk = &test->vk;
    vk_init(vk, NULL);

    if (!vk->vulkan_14_features.dynamicRenderingLocalRead)
        vk_die("dynamicRenderingLocalRead feature is required");

    gbuf_test_init_rt(test);
    gbuf_test_init_pipeline(test);
    gbuf_test_init_descriptor_set(test);
}

static void
gbuf_test_cleanup(struct gbuf_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_descriptor_set(vk, test->set);
    vk_destroy_pipeline(vk, test->pipeline);

    vk_destroy_image(vk, test->rt_gbuf);
    vk_destroy_image(vk, test->rt);

    vk_cleanup(vk);
}

static void
gbuf_test_draw_pre(struct gbuf_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageMemoryBarrier2 barriers[2] = {
        {
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
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ,
            .image = test->rt_gbuf->img,
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
gbuf_test_draw_post(struct gbuf_test *test, VkCommandBuffer cmd)
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
gbuf_test_draw_triangle(struct gbuf_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    vk->CmdBeginRendering(cmd, &test->rendering_info);

    vk_bind_pipeline(vk, test->pipeline, cmd);

    const VkBindDescriptorSetsInfo bind_info = {
        .sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO,
        .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
        .layout = test->pipeline->layout,
        .descriptorSetCount = 1,
        .pDescriptorSets = &test->set->set,
    };
    vk->CmdBindDescriptorSets2(cmd, &bind_info);

    vk->CmdDraw(cmd, 3, 1, 0, 0);

    vk->CmdEndRendering(cmd);
}

static void
gbuf_test_draw(struct gbuf_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    gbuf_test_draw_pre(test, cmd);
    gbuf_test_draw_triangle(test, cmd);
    gbuf_test_draw_post(test, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    vk_dump_image(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT, "rt.ppm");
}

int
main(void)
{
    struct gbuf_test test = {
        .color_format = VK_FORMAT_B8G8R8A8_UNORM,
        .width = 300,
        .height = 300,
    };

    gbuf_test_init(&test);
    gbuf_test_draw(&test);
    gbuf_test_cleanup(&test);

    return 0;
}
