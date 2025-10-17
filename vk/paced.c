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
    uint32_t interval_ms;
    uint32_t busy_ms;

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
    struct vk *vk = &test->vk;

    vk_log("interval: %dms", test->interval_ms);
    vk_log("busy: %dms", test->busy_ms);

    if (test->interval_ms == test->busy_ms) {
        while (true)
            paced_test_draw_once(test);
        return;
    }

    /* calibrate draw time */
    uint32_t draw_count = 0;
    uint64_t begin = u_now();
    while (true) {
        paced_test_draw_once(test);
        draw_count++;
        const uint32_t dur_ms = (u_now() - begin) / 1000 / 1000;
        if (dur_ms > 500)
            break;
    }
    vk_wait(vk);
    const uint32_t draw_ns = (u_now() - begin) / draw_count;

    draw_count = test->busy_ms * 1000 * 1000 / draw_ns;
    if (!draw_count)
        draw_count = 1;

    vk_log("calibrated draw time: %d.%dms", (draw_ns / 1000) / 1000,
           (draw_ns / 1000) % 1000);
    vk_log("calibrated draw count: %d", draw_count);

    begin = u_now();
    while (true) {
        for (uint32_t i = 0; i < draw_count; i++)
            paced_test_draw_once(test);

        const uint64_t dur_ms = (u_now() - begin) / 1000 / 1000;
        if (dur_ms < test->interval_ms)
            u_sleep(test->interval_ms - dur_ms);
        begin = u_now();
    }
}

int
main(int argc, char **argv)
{
    struct paced_test test = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .width = 1024,
        .height = 1024,
        .copy_count = 10,

        .interval_ms = 16,
        .busy_ms = 8,
    };

    if (argc > 1)
        test.interval_ms = atoi(argv[1]);
    if (argc > 2)
        test.busy_ms = atoi(argv[2]);

    paced_test_init(&test);
    paced_test_draw(&test);
    paced_test_cleanup(&test);

    return 0;
}
