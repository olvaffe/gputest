/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

struct mem_alloc_test {
    VkDeviceSize size;
    uint32_t count;
    uint32_t mt;

    struct vk vk;
};

static void
mem_alloc_test_init(struct mem_alloc_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);
}

static void
mem_alloc_test_cleanup(struct mem_alloc_test *test)
{
    struct vk *vk = &test->vk;
    vk_cleanup(vk);
}

static void
mem_alloc_test_draw(struct mem_alloc_test *test)
{
    struct vk *vk = &test->vk;

    VkDeviceMemory *mems = malloc(sizeof(*mems) * test->count);
    if (!mems)
        vk_die("failed to alloc mems");

    const uint64_t begin = u_now();
    for (uint32_t i = 0; i < test->count; i++)
        mems[i] = vk_alloc_memory(vk, test->size, test->mt);
    const uint64_t end = u_now();

    vk_log("allocating %u %u MiB VkDeviceMemory took %uus", test->count, (unsigned)(test->size / 1024 / 1024),
           (unsigned)((end - begin) / 1000));

    for (uint32_t i = 0; i < test->count; i++)
        vk->FreeMemory(vk->dev, mems[i], NULL);

    free(mems);
}

int
main(int argc, char **argv)
{
    struct mem_alloc_test test = {
        .size = 4 * 1024 * 1024,
        .count = 256,
        .mt = 0,
    };

    mem_alloc_test_init(&test);
    mem_alloc_test_draw(&test);
    mem_alloc_test_cleanup(&test);

    return 0;
}
