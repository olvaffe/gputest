/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

struct paced_test {
    VkFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t copy_count;
    uint32_t refresh_rate;
    uint32_t utilization;

    struct vk vk;
    struct vk_image *src;
    struct vk_image *dst;
};

static void
paced_test_init_image(struct paced_test *test)
{
    struct vk *vk = &test->vk;

    test->src =
        vk_create_image(vk, test->format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    test->dst =
        vk_create_image(vk, test->format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
}

static void
paced_test_init(struct paced_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);
    paced_test_init_image(test);
}

static void
paced_test_cleanup(struct paced_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_image(vk, test->src);
    vk_destroy_image(vk, test->dst);
    vk_cleanup(vk);
}

static void
paced_test_draw_once(struct paced_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    const VkImageCopy region = {
        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .extent = {
            .width = test->width,
            .height = test->height,
            .depth = 1,
        },
    };
    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier pre_barriers[2] = {
        [0] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .image = test->src->img,
            .subresourceRange = subres_range,
        },
        [1] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = test->dst->img,
            .subresourceRange = subres_range,
        },
    };
    const VkImageMemoryBarrier post_barriers[2] = {
        [0] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .dstAccessMask = VK_ACCESS_HOST_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .image = test->src->img,
            .subresourceRange = subres_range,
        },
        [1] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .image = test->dst->img,
            .subresourceRange = subres_range,
        },
    };

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0, 0, NULL, 0, NULL, ARRAY_SIZE(pre_barriers), pre_barriers);

    for (uint32_t i = 0; i < test->copy_count; i++) {
        vk->CmdCopyImage(cmd, test->src->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         test->dst->img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                           NULL, 0, NULL, ARRAY_SIZE(post_barriers), post_barriers);

    vk_end_cmd(vk);
}

static void
paced_test_draw(struct paced_test *test)
{
    const uint64_t interval = 1000ull * 1000 * 1000 / test->refresh_rate;
    const uint64_t busy = interval * 100 / test->utilization;

    uint64_t begin = u_now();
    while (true) {
        paced_test_draw_once(test);

        if (interval == busy)
            continue;

        const uint64_t dur = u_now() - begin;
        if (dur > busy) {
            u_sleep(interval - dur);
            begin = u_now();
        }
    }
}

int
main(void)
{
    struct paced_test test = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .width = 1024,
        .height = 1024,
        .copy_count = 10,

        .refresh_rate = 60,
        .utilization = 50,
    };

    paced_test_init(&test);
    paced_test_draw(&test);
    paced_test_cleanup(&test);

    return 0;
}
