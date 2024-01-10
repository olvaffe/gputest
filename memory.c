/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

struct memory_test {
    VkFormat format;
    uint32_t width;
    uint32_t height;

    struct vk vk;
};

static void
memory_test_init(struct memory_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);
}

static void
memory_test_cleanup(struct memory_test *test)
{
    struct vk *vk = &test->vk;
    vk_cleanup(vk);
}

static void
memory_test_timed_memcpy(void *dst, const void *src, int size, const char *what)
{
    for (int i = 0; i < 3; i++) {
        const uint64_t begin = vk_now();
        memcpy(dst, src, size);
        const uint64_t end = vk_now();

        const int us = (end - begin) / 1000;
        vk_log("%s iter %d took %d.%dms", what, i, us / 1000, us % 1000);
    }
}

static void
memory_test_draw(struct memory_test *test)
{
    struct vk *vk = &test->vk;
    int size;
    void *dst;

    {
        struct vk_image *img =
            vk_create_image(vk, test->format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                            VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT);

        size = img->mem_size;
        vk_log("testing memcpy of size %d", size);
        dst = malloc(size);
        if (!dst)
            vk_die("failed to alloc dst");

        if (img->mem_mappable) {
            void *src;
            vk->result = vk->MapMemory(vk->dev, img->mem, 0, img->mem_size, 0, &src);
            vk_check(vk, "failed to map image memory");

            memory_test_timed_memcpy(dst, src, size, "linear image");

            vk->UnmapMemory(vk->dev, img->mem);
        }

        vk_destroy_image(vk, img);
    }

    {
        void *src = malloc(size);
        if (!src)
            vk_die("failed to alloc src");
        memory_test_timed_memcpy(dst, src, size, "malloc");
        free(src);
    }

    {
        void *src = calloc(1, size);
        if (!src)
            vk_die("failed to alloc src");
        memory_test_timed_memcpy(dst, src, size, "calloc");
        free(src);
    }

    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        const VkMemoryType *mt = &vk->mem_props.memoryTypes[i];

        if (!(mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            vk_log("mt %d is not host-visible", i);
            continue;
        }

        VkDeviceMemory mem = vk_alloc_memory(vk, size, i);
        void *src;
        vk->result = vk->MapMemory(vk->dev, mem, 0, size, 0, &src);
        vk_check(vk, "failed to map memory");

        char desc[64];
        snprintf(desc, sizeof(desc), "memory type %d (%s%s%s)", i,
                 (mt->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? "Lo" : "..",
                 (mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? "Co" : "..",
                 (mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? "Ca" : "..");

        memory_test_timed_memcpy(dst, src, size, desc);

        vk->FreeMemory(vk->dev, mem, NULL);
    }

    free(dst);
}

int
main(void)
{
    struct memory_test test = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .width = 1080,
        .height = 1080,
    };

    memory_test_init(&test);
    memory_test_draw(&test);
    memory_test_cleanup(&test);

    return 0;
}
