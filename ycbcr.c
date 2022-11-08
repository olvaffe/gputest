/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static const uint32_t ycbcr_test_vs[] = {
#include "ycbcr_test.vert.inc"
};

static const uint32_t ycbcr_test_fs[] = {
#include "ycbcr_test.frag.inc"
};

/* for ycbcr_test_ppm */
#include "ycbcr_test.ppm.inc"

static const float ycbcr_test_vertices[][2] = {
    { -1.0f, 1.0f },
    { 1.0f, 1.0f },
    { -1.0f, -1.0f },
    { 1.0f, -1.0f },
};

struct ycbcr_test {
    VkFormat color_format;
    uint32_t width;
    uint32_t height;
    bool planar;
    VkFilter minmag_filter;
    VkChromaLocation chroma_loc;
    VkFilter chroma_filter;

    struct vk vk;
    struct vk_buffer *vb;

    struct vk_image *tex;

    struct vk_image *rt;
    struct vk_framebuffer *fb;

    struct vk_pipeline *pipeline;
    struct vk_descriptor_set *set;
};

static void
ycbcr_test_init_descriptor_set(struct ycbcr_test *test)
{
    struct vk *vk = &test->vk;

    test->set = vk_create_descriptor_set(vk, test->pipeline->set_layouts[0]);
    vk_write_descriptor_set_image(vk, test->set, test->tex);
}

static void
ycbcr_test_init_pipeline(struct ycbcr_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, ycbcr_test_vs,
                           sizeof(ycbcr_test_vs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, ycbcr_test_fs,
                           sizeof(ycbcr_test_fs));

    vk_add_pipeline_set_layout(vk, test->pipeline, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                               VK_SHADER_STAGE_FRAGMENT_BIT, &test->tex->sampler);

    const uint32_t comp_count = ARRAY_SIZE(ycbcr_test_vertices[0]);
    vk_set_pipeline_vertices(vk, test->pipeline, &comp_count, 1);

    vk_set_pipeline_topology(vk, test->pipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_FILL);

    vk_setup_pipeline(vk, test->pipeline, test->fb);
    vk_compile_pipeline(vk, test->pipeline);
}

static void
ycbcr_test_init_framebuffer(struct ycbcr_test *test)
{
    struct vk *vk = &test->vk;

    test->rt =
        vk_create_image(vk, test->color_format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    vk_create_image_render_view(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT);

    test->fb = vk_create_framebuffer(vk, test->rt, NULL, NULL);
}

static void
ycbcr_test_init_texture(struct ycbcr_test *test)
{
    struct vk *vk = &test->vk;

    test->tex =
        vk_create_image_from_ppm(vk, ycbcr_test_ppm, ARRAY_SIZE(ycbcr_test_ppm), test->planar);
    if (test->planar) {
        if (test->chroma_filter != test->minmag_filter &&
            !(test->tex->features &
              VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT))
            vk_die("chroma filter and min/mag filter must be the same");

        vk_create_image_ycbcr_conversion(vk, test->tex, test->chroma_loc, test->chroma_filter);
    }
    vk_create_image_sample_view(vk, test->tex, VK_IMAGE_ASPECT_COLOR_BIT, test->minmag_filter);
}

static void
ycbcr_test_init_vb(struct ycbcr_test *test)
{
    struct vk *vk = &test->vk;

    test->vb =
        vk_create_buffer(vk, sizeof(ycbcr_test_vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    memcpy(test->vb->mem_ptr, ycbcr_test_vertices, sizeof(ycbcr_test_vertices));
}

static void
ycbcr_test_init(struct ycbcr_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk);
    ycbcr_test_init_vb(test);

    ycbcr_test_init_texture(test);
    ycbcr_test_init_framebuffer(test);
    ycbcr_test_init_pipeline(test);
    ycbcr_test_init_descriptor_set(test);
}

static void
ycbcr_test_cleanup(struct ycbcr_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_descriptor_set(vk, test->set);
    vk_destroy_pipeline(vk, test->pipeline);

    vk_destroy_image(vk, test->rt);
    vk_destroy_framebuffer(vk, test->fb);

    vk_destroy_image(vk, test->tex);

    vk_destroy_buffer(vk, test->vb);

    vk_cleanup(vk);
}

static void
ycbcr_test_draw_triangle(struct ycbcr_test *test, VkCommandBuffer cmd)
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

    vk->CmdDraw(cmd, ARRAY_SIZE(ycbcr_test_vertices), 1, 0, 0);

    vk->CmdEndRenderPass(cmd);

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 0, NULL, 1, &barrier2);
}

static void
ycbcr_test_draw_prep_texture(struct ycbcr_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image = test->tex->img,
        .subresourceRange = subres_range,
    };

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           0, 0, NULL, 0, NULL, 1, &barrier);
}

static void
ycbcr_test_draw(struct ycbcr_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk);

    ycbcr_test_draw_prep_texture(test, cmd);
    ycbcr_test_draw_triangle(test, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    vk_dump_image(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT, "rt.ppm");
}

int
main(int argc, const char **argv)
{
    struct ycbcr_test test = {
        .color_format = VK_FORMAT_B8G8R8A8_UNORM,
        .width = 300,
        .height = 300,
        .planar = true,
        .minmag_filter = VK_FILTER_NEAREST,
        .chroma_loc = VK_CHROMA_LOCATION_MIDPOINT,
        .chroma_filter = VK_FILTER_NEAREST,
    };

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "planar"))
            test.planar = true;
        else if (!strcmp(argv[i], "rgb"))
            test.planar = false;
        else if (!strcmp(argv[i], "minmag_nearest"))
            test.minmag_filter = VK_FILTER_NEAREST;
        else if (!strcmp(argv[i], "minmag_linear"))
            test.minmag_filter = VK_FILTER_LINEAR;
        else if (!strcmp(argv[i], "midpoint"))
            test.chroma_loc = VK_CHROMA_LOCATION_MIDPOINT;
        else if (!strcmp(argv[i], "cosited"))
            test.chroma_loc = VK_CHROMA_LOCATION_COSITED_EVEN;
        else if (!strcmp(argv[i], "chroma_nearest"))
            test.chroma_filter = VK_FILTER_NEAREST;
        else if (!strcmp(argv[i], "chroma_linear"))
            test.chroma_filter = VK_FILTER_LINEAR;
        else
            vk_die("unknown option %s", argv[i]);
    }

    ycbcr_test_init(&test);
    ycbcr_test_draw(&test);
    ycbcr_test_cleanup(&test);

    return 0;
}
