/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

struct memory_test {
    VkFormat format;
    uint32_t width;
    uint32_t height;

    int loop;
    int bench_mt;

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
memory_test_timed_memcpy(struct memory_test *test,
                         const VkMappedMemoryRange *invalidate,
                         void *dst,
                         const void *src,
                         int size,
                         const char *what)
{
    struct vk *vk = &test->vk;

    if (test->bench_mt == -1) {
        for (int i = 0; i < test->loop; i++) {
            const uint64_t begin = u_now();
            if (invalidate)
                vk->InvalidateMappedMemoryRanges(vk->dev, 1, invalidate);
            memcpy(dst, src, size);
            const uint64_t end = u_now();

            const int us = (end - begin) / 1000;
            vk_log("%s iter %d took %d.%dms", what, i, us / 1000, us % 1000);
        }
    } else {
        const uint64_t begin = u_now();
        for (int i = 0; i < test->loop; i++) {
            if (invalidate)
                vk->InvalidateMappedMemoryRanges(vk->dev, 1, invalidate);
            memcpy(dst, src, size);
        }
        const uint64_t end = u_now();

        const uint64_t us = (end - begin) / 1000;
        const int avg = us / test->loop;
        vk_log("%s took %d.%dms on average (total %d iters)", what, avg / 1000, avg % 1000,
               test->loop);
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

        if (test->bench_mt == -1 && img->mem_mappable) {
            void *src;
            vk->result = vk->MapMemory(vk->dev, img->mem, 0, img->mem_size, 0, &src);
            vk_check(vk, "failed to map image memory");

            memory_test_timed_memcpy(test, NULL, dst, src, size, "linear image");

            vk->UnmapMemory(vk->dev, img->mem);
        }

        vk_destroy_image(vk, img);
    }

    if (test->bench_mt == -1) {
        void *src = malloc(size);
        if (!src)
            vk_die("failed to alloc src");
        memory_test_timed_memcpy(test, NULL, dst, src, size, "malloc");
        free(src);
    }

    if (test->bench_mt == -1) {
        void *src = calloc(1, size);
        if (!src)
            vk_die("failed to alloc src");
        memory_test_timed_memcpy(test, NULL, dst, src, size, "calloc");
        free(src);
    }

    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        const VkMemoryType *mt = &vk->mem_props.memoryTypes[i];

        if (test->bench_mt >= 0 && test->bench_mt != (int)i)
            continue;

        if (!(mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            vk_log("mt %d is not host-visible", i);
            continue;
        }

        VkDeviceMemory mem = vk_alloc_memory(vk, size, i);
        void *src;
        vk->result = vk->MapMemory(vk->dev, mem, 0, size, 0, &src);
        vk_check(vk, "failed to map memory");

        const bool mt_local = mt->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        const bool mt_coherent = mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        const bool mt_cached = mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        char desc[64];
        snprintf(desc, sizeof(desc), "memory type %d (%s%s%s)", i, mt_local ? "Lo" : "..",
                 mt_coherent ? "Co" : "..", mt_cached ? "Ca" : "..");

        const VkMappedMemoryRange invalidate = {
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = mem,
            .size = size,
        };

        memory_test_timed_memcpy(test, mt_coherent ? NULL : &invalidate, dst, src, size, desc);

        vk->FreeMemory(vk->dev, mem, NULL);
    }

    free(dst);
}

int
main(int argc, char **argv)
{
    struct memory_test test = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .width = 1080,
        .height = 1080,

        .loop = 3,
        .bench_mt = -1,
    };

    if (argc == 3) {
        test.loop = atoi(argv[1]);
        test.bench_mt = atoi(argv[2]);
    } else if (argc != 1) {
        vk_die("usage: %s [<loop> <mt>]", argv[0]);
    }

    memory_test_init(&test);
    memory_test_draw(&test);
    memory_test_cleanup(&test);

    return 0;
}
