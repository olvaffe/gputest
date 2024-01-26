/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

struct image_test {
    VkFormat format;
    uint32_t width;
    uint32_t height;

    struct vk vk;
};

static void
image_test_init(struct image_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);
}

static void
image_test_cleanup(struct image_test *test)
{
    struct vk *vk = &test->vk;
    vk_cleanup(vk);
}

static void
image_test_draw(struct image_test *test)
{
    struct vk *vk = &test->vk;

    const VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    struct vk_image *img = vk_create_image(vk, test->format, test->width, test->height,
                                           VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_LINEAR, usage);

    const VkImageSubresource subres = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    };
    VkSubresourceLayout layout;
    vk->GetImageSubresourceLayout(vk->dev, img->img, &subres, &layout);

    vk_log("image %dx%d format %d usage 0x%x: offset %d size %d rowPitch %d mem %d", test->width,
           test->height, test->format, usage, (int)layout.offset, (int)layout.size,
           (int)layout.rowPitch, (int)img->mem_size);

    vk_destroy_image(vk, img);
}

int
main(int argc, char **argv)
{
    struct image_test test = {
        .format = VK_FORMAT_R5G6B5_UNORM_PACK16,
        .width = 300,
        .height = 300,
    };

    if (argc == 3) {
        test.width = atoi(argv[1]);
        test.height = atoi(argv[2]);
    } else if (argc != 1) {
        vk_die("usage: %s [<width> <height>]", argv[0]);
    }

    image_test_init(&test);
    image_test_draw(&test);
    image_test_cleanup(&test);

    return 0;
}
