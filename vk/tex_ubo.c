/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

/* This test draws a textured and rotated triangle to a linear color image and
 * dumps it to a file.  The texture image is also linear and is also dumped.
 *
 * The texture image is cleared to a solid color.  A render pass is used to
 * clear the color image and draw the triangle.
 */

#include "vkutil.h"

static const uint32_t tex_ubo_test_vs[] = {
#include "tex_ubo_test.vert.inc"
};

static const uint32_t tex_ubo_test_fs[] = {
#include "tex_ubo_test.frag.inc"
};

static const float tex_ubo_test_vertices[6][2] = {
    /* tri1 */
    { -1.0f, -1.0f },
    { 0.0f, 0.0f },
    { 1.0f, -1.0f },
    /* tri2 */
    { -1.0f, 1.0f },
    { 1.0f, 1.0f },
    { 0.0f, 0.0f },
};

/* note that std140 requires vec4 alignment */
static const float tex_ubo_test_color_scales[2][4] = {
    /* tri1 color scale */
    [0] = { 1.0f, 1.0f, 1.0f, 1.0f },
    /* tri2 color scale */
    [1] = { 0.3f, 0.3f, 0.3f, 0.3f },
};

struct tex_ubo_test {
    VkFormat color_format;
    VkFormat tex_format;
    uint32_t width;
    uint32_t height;

    struct vk vk;
    struct vk_buffer *vb;

    struct vk_image *tex;
    struct vk_buffer *ubo;

    struct vk_image *rt;
    VkRenderingAttachmentInfo color_att;
    VkRenderingInfo rendering_info;

    struct vk_pipeline *pipeline;
    struct vk_descriptor_set *tex_set;
    struct vk_descriptor_set *ubo_set;
};

static void
tex_ubo_test_init_descriptor_sets(struct tex_ubo_test *test)
{
    struct vk *vk = &test->vk;

    test->tex_set = vk_create_descriptor_set(vk, test->pipeline->set_layouts[0]);
    vk_write_descriptor_set_image(vk, test->tex_set, test->tex);

    test->ubo_set = vk_create_descriptor_set(vk, test->pipeline->set_layouts[1]);
    vk_write_descriptor_set_buffer(vk, test->ubo_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                   test->ubo, sizeof(tex_ubo_test_color_scales[0]));
}

static void
tex_ubo_test_init_pipeline(struct tex_ubo_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, tex_ubo_test_vs,
                           sizeof(tex_ubo_test_vs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, tex_ubo_test_fs,
                           sizeof(tex_ubo_test_fs));

    vk_add_pipeline_set_layout(vk, test->pipeline, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                               VK_SHADER_STAGE_FRAGMENT_BIT, NULL);
    vk_add_pipeline_set_layout(vk, test->pipeline, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
                               VK_SHADER_STAGE_FRAGMENT_BIT, NULL);

    const uint32_t comp_count = ARRAY_SIZE(tex_ubo_test_vertices[0]);
    vk_set_pipeline_vertices(vk, test->pipeline, &comp_count, 1);
    test->pipeline->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    vk_set_pipeline_viewport(vk, test->pipeline, test->width, test->height);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_FILL, false);
    test->pipeline->rendering_info = (VkPipelineRenderingCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &test->color_format,
    };
    vk_compile_pipeline(vk, test->pipeline);
}

static void
tex_ubo_test_init_framebuffer(struct tex_ubo_test *test)
{
    struct vk *vk = &test->vk;

    test->rt =
        vk_create_image(vk, test->color_format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    vk_create_image_render_view(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT);

    test->color_att = (VkRenderingAttachmentInfo){
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
tex_ubo_test_init_ubo(struct tex_ubo_test *test)
{
    struct vk *vk = &test->vk;

    test->ubo = vk_create_buffer(vk, 0, sizeof(tex_ubo_test_color_scales),
                                 VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT);
    memcpy(test->ubo->mem_ptr, tex_ubo_test_color_scales, sizeof(tex_ubo_test_color_scales));
}

static void
tex_ubo_test_init_texture(struct tex_ubo_test *test)
{
    struct vk *vk = &test->vk;

    test->tex = vk_create_image(vk, test->tex_format, test->width, test->height,
                                VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_LINEAR,
                                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    vk_create_image_sample_view(vk, test->tex, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_create_image_sampler(vk, test->tex, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST);
}

static void
tex_ubo_test_init_vb(struct tex_ubo_test *test)
{
    struct vk *vk = &test->vk;

    test->vb = vk_create_buffer(vk, 0, sizeof(tex_ubo_test_vertices),
                                VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT);
    memcpy(test->vb->mem_ptr, tex_ubo_test_vertices, sizeof(tex_ubo_test_vertices));
}

static void
tex_ubo_test_init(struct tex_ubo_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);
    tex_ubo_test_init_vb(test);

    tex_ubo_test_init_texture(test);
    tex_ubo_test_init_ubo(test);
    tex_ubo_test_init_framebuffer(test);
    tex_ubo_test_init_pipeline(test);
    tex_ubo_test_init_descriptor_sets(test);
}

static void
tex_ubo_test_cleanup(struct tex_ubo_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_descriptor_set(vk, test->tex_set);
    vk_destroy_descriptor_set(vk, test->ubo_set);
    vk_destroy_pipeline(vk, test->pipeline);

    vk_destroy_image(vk, test->rt);

    vk_destroy_image(vk, test->tex);
    vk_destroy_buffer(vk, test->ubo);

    vk_destroy_buffer(vk, test->vb);

    vk_cleanup(vk);
}

static void
tex_ubo_test_draw_pre(struct tex_ubo_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageMemoryBarrier2 barrier = {
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
    };

    const VkDependencyInfo dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info);
}

static void
tex_ubo_test_draw_post(struct tex_ubo_test *test, VkCommandBuffer cmd)
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
tex_ubo_test_draw_triangles(struct tex_ubo_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    vk->CmdBeginRendering(cmd, &test->rendering_info);

    vk->CmdBindVertexBuffers2(cmd, 0, 1, &test->vb->buf, &(VkDeviceSize){ 0 }, NULL, NULL);
    vk_bind_pipeline(vk, test->pipeline, cmd);

    const VkBindDescriptorSetsInfo tex_bind_info = {
        .sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO,
        .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
        .layout = test->pipeline->pipeline_layout,
        .descriptorSetCount = 1,
        .pDescriptorSets = &test->tex_set->set,
    };
    vk->CmdBindDescriptorSets2(cmd, &tex_bind_info);

    const VkBindDescriptorSetsInfo ubo_bind_info_0 = {
        .sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO,
        .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
        .layout = test->pipeline->pipeline_layout,
        .firstSet = 1,
        .descriptorSetCount = 1,
        .pDescriptorSets = &test->ubo_set->set,
        .dynamicOffsetCount = 1,
        .pDynamicOffsets = &(uint32_t){ 0 },
    };
    vk->CmdBindDescriptorSets2(cmd, &ubo_bind_info_0);
    vk->CmdDraw(cmd, 3, 1, 0, 0);

    vk->CmdBindDescriptorSets2(cmd, &tex_bind_info);

    const VkBindDescriptorSetsInfo ubo_bind_info_1 = {
        .sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO,
        .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
        .layout = test->pipeline->pipeline_layout,
        .firstSet = 1,
        .descriptorSetCount = 1,
        .pDescriptorSets = &test->ubo_set->set,
        .dynamicOffsetCount = 1,
        .pDynamicOffsets = &(uint32_t){ sizeof(tex_ubo_test_color_scales[0]) },
    };
    vk->CmdBindDescriptorSets2(cmd, &ubo_bind_info_1);
    vk->CmdDraw(cmd, 3, 1, 3, 0);

    vk->CmdEndRendering(cmd);
}

static void
tex_ubo_test_draw_prep_texture(struct tex_ubo_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier2 barrier1 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = test->tex->img,
        .subresourceRange = subres_range,
    };
    const VkImageMemoryBarrier2 barrier2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image = test->tex->img,
        .subresourceRange = subres_range,
    };
    const VkClearColorValue clear_val = {
        .float32 = { 0.25f, 0.50f, 0.75f, 1.00f },
    };

    const VkDependencyInfo dep_info3 = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier1,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info3);
    vk->CmdClearColorImage(cmd, test->tex->img, barrier1.newLayout, &clear_val, 1, &subres_range);
    const VkDependencyInfo dep_info4 = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier2,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info4);
}

static void
tex_ubo_test_draw(struct tex_ubo_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    tex_ubo_test_draw_prep_texture(test, cmd);
    tex_ubo_test_draw_pre(test, cmd);
    tex_ubo_test_draw_triangles(test, cmd);
    tex_ubo_test_draw_post(test, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    vk_dump_image(vk, test->tex, VK_IMAGE_ASPECT_COLOR_BIT, "tex.ppm");
    vk_dump_image(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT, "rt.ppm");
}

int
main(void)
{
    struct tex_ubo_test test = {
        .color_format = VK_FORMAT_B8G8R8A8_UNORM,
        .tex_format = VK_FORMAT_R5G5B5A1_UNORM_PACK16,
        .width = 300,
        .height = 300,
    };

    tex_ubo_test_init(&test);
    tex_ubo_test_draw(&test);
    tex_ubo_test_cleanup(&test);

    return 0;
}
