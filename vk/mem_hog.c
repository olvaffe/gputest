/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

#include <stdatomic.h>
#include <threads.h>

struct mem_hog_test {
    struct vk vk;

    VkDeviceSize buf_size;
    uint32_t buf_count;
    struct vk_buffer **bufs;

    bool sys_enable;
    size_t sys_size;
    uint32_t sys_count;
    void **sys;
    size_t sys_page_size;
    uint32_t sys_page_count;

    thrd_t thread;
    atomic_bool stop;
};

static void
mem_hog_test_init_sys(struct mem_hog_test *test)
{
    if (!test->sys_enable)
        return;

    test->sys = calloc(test->sys_count, sizeof(*test->sys));
    if (!test->sys)
        vk_die("failed to alloc sys");

    for (uint32_t i = 0; i < test->sys_count; i++) {
        test->sys[i] = malloc(test->sys_size);
        if (!test->sys[i])
            vk_die("failed to alloc sys[%d]", i);
    }

    test->sys_page_size = sysconf(_SC_PAGESIZE);
    test->sys_page_count = test->sys_size / test->sys_page_size;
}

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
    mem_hog_test_init_sys(test);
}

static void
mem_hog_test_cleanup(struct mem_hog_test *test)
{
    struct vk *vk = &test->vk;

    if (test->sys_enable) {
        for (uint32_t i = 0; i < test->sys_count; i++)
            free(test->sys[i]);
        free(test->sys);
    }

    for (uint32_t i = 0; i < test->buf_count; i++)
        vk_destroy_buffer(vk, test->bufs[i]);
    free(test->bufs);

    vk_cleanup(vk);
}

static int
mem_hog_test_thread(void *arg)
{
    struct mem_hog_test *test = arg;

    while (!atomic_load(&test->stop)) {
        for (uint32_t i = 0; i < test->sys_count; i++) {
            for (uint32_t j = 0; j < test->sys_page_count; j++)
                memset(test->sys[i] + test->sys_page_size * j, 0x37, 64);
        }

        u_sleep(5);
    }

    return 0;
}
static void
mem_hog_test_draw(struct mem_hog_test *test)
{
    struct vk *vk = &test->vk;

    const float buf_mb = test->buf_size / 1024.0f / 1024.0f;
    const float total_buf_gb = buf_mb * test->buf_count / 1024.0f;
    vk_log("buf size %.1fMiB, buf count %u, total buf size %.1fGiB", buf_mb, test->buf_count,
           total_buf_gb);

    if (test->sys_enable) {
        const float sys_mb = test->sys_size / 1024.0f / 1024.0f;
        const float total_sys_gb = sys_mb * test->sys_count / 1024.0f;
        vk_log("sys size %.1fMiB, sys count %u, total sys size %.1fGiB", sys_mb, test->sys_count,
               total_sys_gb);

        if (thrd_create(&test->thread, mem_hog_test_thread, test) != thrd_success)
            vk_die("failed to create thread");
    }

    while (true) {
        VkCommandBuffer cmd = vk_begin_cmd(vk, false);

        for (uint32_t i = 0; i < test->buf_count; i++)
            vk->CmdFillBuffer(cmd, test->bufs[i]->buf, 0, 64, 0x37);

        vk_end_cmd(vk);
    }

    if (test->sys_enable) {
        atomic_store(&test->stop, true);
        if (thrd_join(test->thread, NULL) != thrd_success)
            vk_die("failed to join thread");
    }
}

int
main(int argc, char **argv)
{
    struct mem_hog_test test = {
        .buf_size = 1ull * 1024 * 1024,
        .buf_count = 1024,

        .sys_enable = true,
        .sys_size = 1ull * 1024 * 1024,
        .sys_count = 1024,
    };

    if (argc > 1) {
        test.buf_count = atoi(argv[1]);
        test.sys_count = test.buf_count;
    }

    if (!test.sys_enable) {
        test.sys_size = 0;
        test.sys_count = 0;
    }

    mem_hog_test_init(&test);
    mem_hog_test_draw(&test);
    mem_hog_test_cleanup(&test);

    return 0;
}
