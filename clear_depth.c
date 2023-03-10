/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

struct clear_depth_test {
    VkFormat depth_format;
    uint32_t width;
    uint32_t height;
    VkImageAspectFlags aspect_mask;

    struct vk vk;

    struct vk_image *img;

    struct vk_buffer *buf;
};

static void
clear_depth_test_init(struct clear_depth_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);

    test->img =
        vk_create_image(vk, test->depth_format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    test->buf =
        vk_create_buffer(vk, test->width * test->height, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
}

static void
clear_depth_test_cleanup(struct clear_depth_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_buffer(vk, test->buf);
    vk_destroy_image(vk, test->img);
    vk_cleanup(vk);
}

static void
clear_depth_test_clear(struct clear_depth_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageSubresourceRange subres_range = {
        .aspectMask = test->aspect_mask,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier barrier1 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = test->img->img,
        .subresourceRange = subres_range,
    };
    const VkImageMemoryBarrier barrier2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = test->img->img,
        .subresourceRange = subres_range,
    };
    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0, 0, NULL, 0, NULL, 1, &barrier1);

    const VkClearDepthStencilValue clear_val = {
        .depth = 0.5f,
        .stencil = 8,
    };
    vk->CmdClearDepthStencilImage(cmd, test->img->img, barrier1.newLayout, &clear_val, 1,
                                  &subres_range);

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                           0, NULL, 0, NULL, 1, &barrier2);

    const VkBufferImageCopy region = {
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT,
            .layerCount = 1,
        },
        .imageExtent = {
            .width = test->width,
            .height = test->height,
            .depth = 1,
        },
    };
    vk->CmdCopyImageToBuffer(cmd, test->img->img, barrier2.newLayout, test->buf->buf, 1, &region);

    const VkBufferMemoryBarrier buf_barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .buffer = test->buf->buf,
        .size = VK_WHOLE_SIZE,
    };
    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                           NULL, 1, &buf_barrier, 0, NULL);
}

static void
clear_depth_test_draw(struct clear_depth_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk);

    clear_depth_test_clear(test, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    vk_dump_buffer_raw(vk, test->buf, "rt.raw");
}

int
main(void)
{
    struct clear_depth_test test = {
        .depth_format = VK_FORMAT_D16_UNORM_S8_UINT,
        .width = 49,
        .height = 13,
        .aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
    };

    clear_depth_test_init(&test);
    clear_depth_test_draw(&test);
    clear_depth_test_cleanup(&test);

    return 0;
}
