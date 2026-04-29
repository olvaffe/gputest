/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

struct mem_hog_test {
    struct vk vk;

    VkDeviceSize buf_size;
    uint32_t buf_count;
    struct vk_buffer **bufs;

    VkDeviceMemory mem;
    void *mem_ptr;
    VkDeviceSize mem_used;

    VkBuffer disturb;
    volatile uint32_t *disturb_ptr;

    VkBuffer src_buf;
    volatile uint32_t *src_buf_ptr;

    struct vk_buffer *buf_with_mem;
    VkBuffer dst_buf;
    volatile uint32_t *dst_buf_ptr;

    struct vk_event *gpu_done;
    struct vk_event *cpu_done;
};

static void
mem_hog_test_init_bufs(struct mem_hog_test *test)
{
    struct vk *vk = &test->vk;

    test->bufs = calloc(test->buf_count, sizeof(*test->bufs));
    if (!test->bufs)
        vk_die("failed to alloc bufs");

    for (uint32_t i = 0; i < test->buf_count; i++)
        test->bufs[i] = vk_create_buffer(vk, 0, test->buf_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
}

static void
mem_hog_test_init(struct mem_hog_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);

    mem_hog_test_init_bufs(test);
}

static void
mem_hog_test_cleanup(struct mem_hog_test *test)
{
    struct vk *vk = &test->vk;

    for (uint32_t i = 0; i < test->buf_count; i++)
        vk_destroy_buffer(vk, test->bufs[i]);

    free(test->bufs);

    vk_cleanup(vk);
}

static void
mem_hog_test_draw(struct mem_hog_test *test)
{
    struct vk *vk = &test->vk;

    const float buf_mb = test->buf_size / 1024.0f / 1024.0f;
    const float total_gb = buf_mb * test->buf_count / 1024.0f;
    vk_log("buf size %.1fMiB, buf count %u, total size %.1fGiB", buf_mb, test->buf_count,
           total_gb);

    while (true) {
        VkCommandBuffer cmd = vk_begin_cmd(vk, false);

        for (uint32_t i = 0; i < test->buf_count; i++)
            vk->CmdFillBuffer(cmd, test->bufs[i]->buf, 0, 64, 0x37);

        vk_end_cmd(vk);
    }
}

int
main(int argc, char **argv)
{
    struct mem_hog_test test = {
        .buf_size = 1ull * 1024 * 1024,
        .buf_count = 4096,
    };

    if (argc > 1)
        test.buf_count = atoi(argv[1]);

    mem_hog_test_init(&test);
    mem_hog_test_draw(&test);
    mem_hog_test_cleanup(&test);

    return 0;
}
