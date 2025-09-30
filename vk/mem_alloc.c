/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

struct mem_alloc_test {
    VkDeviceSize base_size;
    uint32_t order;
    uint32_t loop;
    uint32_t mt;

    struct vk vk;
    VkDeviceMemory *mems;
};

static void
mem_alloc_test_init(struct mem_alloc_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);

    test->mems = malloc(sizeof(*test->mems) * test->order);
    if (!test->mems)
        vk_die("failed to alloc mems");
}

static void
mem_alloc_test_cleanup(struct mem_alloc_test *test)
{
    struct vk *vk = &test->vk;

    free(test->mems);
    vk_cleanup(vk);
}

static void
mem_alloc_test_iterate(struct mem_alloc_test *test)
{
    struct vk *vk = &test->vk;

    for (uint32_t i = 0; i < test->order; i++)
        test->mems[i] = vk_alloc_memory(vk, test->base_size << i, test->mt);

    for (uint32_t i = 0; i < test->order; i++)
        vk->FreeMemory(vk->dev, test->mems[i], NULL);
}

static void
mem_alloc_test_loop(struct mem_alloc_test *test)
{
    /* warm up */
    mem_alloc_test_iterate(test);

    const uint64_t begin = u_now();
    for (uint32_t i = 0; i < test->loop; i++)
        mem_alloc_test_iterate(test);
    const uint64_t end = u_now();

    const uint32_t total_count = test->loop * test->order;
    const VkDeviceSize total_size = test->loop * test->base_size * ((1 << test->order) - 1);
    const uint32_t us = (end - begin) / 1000;

    vk_log("allocating %u VkDeviceMemory of total size %u MiB took %u.%ums", total_count,
           (unsigned)(total_size / 1024 / 1024), us / 1000, us % 1000);
}

int
main(int argc, char **argv)
{
    struct mem_alloc_test test = {
        .base_size = 1 * 1024 * 1024,
        .order = 10,
        .loop = 32,
        .mt = 0,
    };

    mem_alloc_test_init(&test);
    mem_alloc_test_loop(&test);
    mem_alloc_test_cleanup(&test);

    return 0;
}
