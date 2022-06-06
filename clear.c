/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

struct clear_test {
    VkFormat color_format;
    uint32_t width;
    uint32_t height;
    VkImageAspectFlagBits aspect;

    struct vk vk;

    struct vk_image *img;
};

static void
clear_test_init(struct clear_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk);

    test->img =
        vk_create_image(vk, test->color_format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    vk_fill_image(vk, test->img, test->aspect, 0x11);
}

static void
clear_test_cleanup(struct clear_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_image(vk, test->img);
    vk_cleanup(vk);
}

static void
clear_test_clear(struct clear_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageSubresourceRange subres_range = {
        .aspectMask = test->aspect,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier barrier1 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = test->img->img,
        .subresourceRange = subres_range,
    };
    const VkImageMemoryBarrier barrier2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = test->img->img,
        .subresourceRange = subres_range,
    };
    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0, 0, NULL, 0, NULL, 1, &barrier1);

    if (test->aspect & VK_IMAGE_ASPECT_COLOR_BIT) {
        const VkClearColorValue clear_val = {
            .float32 = { 0.25f, 0.50f, 0.75f, 1.00f },
        };

        vk->CmdClearColorImage(cmd, test->img->img, barrier1.newLayout, &clear_val, 1,
                               &subres_range);
    } else {
        const VkClearDepthStencilValue clear_val = {
            .depth = 0.5f,
            .stencil = 8,
        };
        vk->CmdClearDepthStencilImage(cmd, test->img->img, barrier1.newLayout, &clear_val, 1,
                                      &subres_range);
    }

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                           NULL, 0, NULL, 1, &barrier2);
}

static void
clear_test_draw(struct clear_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk);

    clear_test_clear(test, cmd);

    vk_end_cmd(vk);

    vk_dump_image(vk, test->img, test->aspect, "rt.ppm");
}

int
main(void)
{
    struct clear_test test = {
        .color_format = VK_FORMAT_B8G8R8A8_UNORM,
        .width = 300,
        .height = 300,
        .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
    };

    clear_test_init(&test);
    clear_test_draw(&test);
    clear_test_cleanup(&test);

    return 0;
}
