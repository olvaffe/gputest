/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

/* This test clears a tiled depth image and dumps it to a file. */

#include "vkutil.h"

struct clear_rp_test {
    VkFormat format;
    uint32_t width;
    uint32_t height;
    VkImageTiling tiling;
    VkImageUsageFlags usage;
    VkImageAspectFlags aspect_mask;

    bool is_color;

    struct vk vk;

    struct vk_image *img;
    struct vk_framebuffer *fb;
};

static void
clear_rp_test_init_framebuffer(struct clear_rp_test *test)
{
    struct vk *vk = &test->vk;

    test->img = vk_create_image(vk, test->format, test->width, test->height,
                                VK_SAMPLE_COUNT_1_BIT, test->tiling, test->usage);
    vk_create_image_render_view(vk, test->img, test->aspect_mask);

    struct vk_image *color = test->is_color ? test->img : NULL;
    struct vk_image *depth = test->is_color ? NULL : test->img;
    test->fb = vk_create_framebuffer(vk, color, NULL, depth);
}

static void
clear_rp_test_init(struct clear_rp_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk);

    clear_rp_test_init_framebuffer(test);
}

static void
clear_rp_test_cleanup(struct clear_rp_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_image(vk, test->img);
    vk_destroy_framebuffer(vk, test->fb);

    vk_cleanup(vk);
}

static void
clear_rp_test_draw_triangle(struct clear_rp_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageSubresourceRange subres_range = {
        .aspectMask = test->aspect_mask,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = test->is_color ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                        : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = test->is_color ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                    : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .image = test->img->img,
        .subresourceRange = subres_range,
    };

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           test->is_color ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                          : VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                           0, 0, NULL, 0, NULL, 1, &barrier);

    VkClearValue clear_val;
    if (test->is_color) {
        clear_val.color = (VkClearColorValue){ .float32 = { 0.7f, 0.6f, 0.5f, 1.0f } };
    } else {
        clear_val.depthStencil = (VkClearDepthStencilValue){
            .depth = 0.5f,
            .stencil = 10,
        };
    }
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
        .pClearValues = &clear_val,
    };
    vk->CmdBeginRenderPass(cmd, &pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vk->CmdEndRenderPass(cmd);

    if (test->is_color && test->tiling == VK_IMAGE_TILING_LINEAR) {
        const VkImageMemoryBarrier barrier2 = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .image = test->img->img,
            .subresourceRange = subres_range,
        };
        vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                               VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 0, NULL, 1, &barrier2);
    }
}

static void
clear_rp_test_draw(struct clear_rp_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk);

    clear_rp_test_draw_triangle(test, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    if (test->tiling == VK_IMAGE_TILING_LINEAR)
        vk_dump_image(vk, test->img, test->aspect_mask, "rt.ppm");
    else
        vk_dump_image_raw(vk, test->img, "rt.raw");
}

int
main(void)
{
    struct clear_rp_test test = {
        .format = VK_FORMAT_D24_UNORM_S8_UINT,
        .width = 300,
        .height = 300,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
    };

    test.is_color = test.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    clear_rp_test_init(&test);
    clear_rp_test_draw(&test);
    clear_rp_test_cleanup(&test);

    return 0;
}
