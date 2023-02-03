/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

/* This test clears a tiled depth image and dumps it to a file. */

#include "vkutil.h"

struct renderpass_ops_test {
    uint32_t width;
    uint32_t height;

    VkFormat color_format;
    VkImageTiling color_tiling;

    VkFormat depth_format;
    VkImageAspectFlags depth_aspect_mask;

    struct vk vk;

    struct vk_image *color_img;
    struct vk_image *depth_img;
    struct vk_framebuffer *fb;
};

static void
renderpass_ops_test_init_framebuffer(struct renderpass_ops_test *test)
{
    struct vk *vk = &test->vk;

    if (test->color_format != VK_FORMAT_UNDEFINED) {
        test->color_img = vk_create_image(vk, test->color_format, test->width, test->height,
                                          VK_SAMPLE_COUNT_1_BIT, test->color_tiling,
                                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        vk_create_image_render_view(vk, test->color_img, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    if (test->depth_format != VK_FORMAT_UNDEFINED) {
        test->depth_img = vk_create_image(vk, test->depth_format, test->width, test->height,
                                          VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
                                          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        vk_create_image_render_view(vk, test->depth_img, test->depth_aspect_mask);
    }

    test->fb = vk_create_framebuffer(vk, test->color_img, NULL, test->depth_img);
}

static void
renderpass_ops_test_init(struct renderpass_ops_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk);

    renderpass_ops_test_init_framebuffer(test);
}

static void
renderpass_ops_test_cleanup(struct renderpass_ops_test *test)
{
    struct vk *vk = &test->vk;

    if (test->color_img)
        vk_destroy_image(vk, test->color_img);
    if (test->depth_img)
        vk_destroy_image(vk, test->depth_img);
    vk_destroy_framebuffer(vk, test->fb);

    vk_cleanup(vk);
}

static void
renderpass_ops_test_draw_ops(struct renderpass_ops_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    VkImageMemoryBarrier barriers[2];
    VkClearValue clear_vals[2];
    uint32_t att_count = 0;

    if (test->color_format != VK_FORMAT_UNDEFINED) {
        barriers[att_count] = (VkImageMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .image = test->color_img->img,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };

        clear_vals[att_count++] = (VkClearValue) {
            .color = {
                .float32 = { 0.7f, 0.6f, 0.5f, 1.0f },
            },
        };
    }
    if (test->depth_format != VK_FORMAT_UNDEFINED) {
        barriers[att_count] = (VkImageMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .image = test->depth_img->img,
            .subresourceRange = {
                .aspectMask = test->depth_aspect_mask,
                .levelCount = 1,
                .layerCount = 1,

            },
        };

        clear_vals[att_count++] = (VkClearValue){
            .depthStencil = {
                .depth = 0.5f,
                .stencil = 10,
            },
        };
    }

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, att_count,
                           barriers);

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
        .clearValueCount = att_count,
        .pClearValues = clear_vals,
    };

    /* trigger renderpass ops */
    vk->CmdBeginRenderPass(cmd, &pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vk->CmdEndRenderPass(cmd);

    if (test->color_format != VK_FORMAT_UNDEFINED &&
        test->color_tiling == VK_IMAGE_TILING_LINEAR) {
        const VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .image = test->color_img->img,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                               VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    }
}

static void
renderpass_ops_test_draw(struct renderpass_ops_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk);

    renderpass_ops_test_draw_ops(test, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    if (test->color_format != VK_FORMAT_UNDEFINED && test->color_tiling == VK_IMAGE_TILING_LINEAR)
        vk_dump_image(vk, test->color_img, VK_IMAGE_ASPECT_COLOR_BIT, "rt.ppm");

    if (test->depth_format != VK_FORMAT_UNDEFINED)
        vk_dump_image_raw(vk, test->depth_img, "zs.raw");
}

int
main(void)
{
    /* VkImageSubresourceRange has some rules
     *
     *  - aspectMask must be only VK_IMAGE_ASPECT_COLOR_BIT,
     *    VK_IMAGE_ASPECT_DEPTH_BIT or VK_IMAGE_ASPECT_STENCIL_BIT if format
     *    is a color, depth-only or stencil-only format, respectively, except
     *    if format is a multi-planar format.
     *  - If using a depth/stencil format with both depth and stencil
     *    components, aspectMask must include at least one of
     *    VK_IMAGE_ASPECT_DEPTH_BIT and VK_IMAGE_ASPECT_STENCIL_BIT, and can
     *    include both.
     *  - When using an image view of a depth/stencil image to populate a
     *    descriptor set (e.g. for sampling in the shader, or for use as an
     *    input attachment), the aspectMask must only include one bit, which
     *    selects whether the image view is used for depth reads (i.e. using a
     *    floating-point sampler or input attachment in the shader) or stencil
     *    reads (i.e. using an unsigned integer sampler or input attachment in
     *    the shader).
     *  - When an image view of a depth/stencil image is used as a
     *    depth/stencil framebuffer attachment, the aspectMask is ignored and
     *    both depth and stencil image subresources are used.
     */
    struct renderpass_ops_test test = {
        .width = 300,
        .height = 300,

        .color_format = VK_FORMAT_B8G8R8A8_UNORM,
        .color_tiling = VK_IMAGE_TILING_LINEAR,

        .depth_format = VK_FORMAT_D24_UNORM_S8_UINT,
        .depth_aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
    };

    renderpass_ops_test_init(&test);
    renderpass_ops_test_draw(&test);
    renderpass_ops_test_cleanup(&test);

    return 0;
}
