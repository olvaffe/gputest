/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

#include <stdatomic.h>
#include <threads.h>

struct mem_hog_test {
    struct vk vk;

    struct {
        VkDeviceSize size;
        uint32_t count;
        struct vk_buffer **bufs;

        uint32_t sleep;
    } gpu;

    struct {
        size_t size;
        uint32_t count;
        void **bufs;

        uint32_t sleep;

        size_t page_size;
        uint32_t page_count;
    } cpu;

    thrd_t thread;
    atomic_bool stop;
};

static void
mem_hog_test_init_cpu(struct mem_hog_test *test)
{
    if (!test->cpu.count)
        return;

    test->cpu.bufs = calloc(test->cpu.count, sizeof(*test->cpu.bufs));
    if (!test->cpu.bufs)
        vk_die("failed to alloc sys");

    for (uint32_t i = 0; i < test->cpu.count; i++) {
        test->cpu.bufs[i] = malloc(test->cpu.size);
        if (!test->cpu.bufs[i])
            vk_die("failed to alloc sys[%d]", i);
    }

    test->cpu.page_size = sysconf(_SC_PAGESIZE);
    test->cpu.page_count = test->cpu.size / test->cpu.page_size;
}

static void
mem_hog_test_init_gpu(struct mem_hog_test *test)
{
    struct vk *vk = &test->vk;

    if (!test->gpu.count)
        return;

    test->gpu.bufs = calloc(test->gpu.count, sizeof(*test->gpu.bufs));
    if (!test->gpu.bufs)
        vk_die("failed to alloc bufs");

    for (uint32_t i = 0; i < test->gpu.count; i++)
        test->gpu.bufs[i] =
            vk_create_buffer(vk, 0, test->gpu.size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
}

static void
mem_hog_test_init(struct mem_hog_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);

    mem_hog_test_init_gpu(test);
    mem_hog_test_init_cpu(test);
}

static void
mem_hog_test_cleanup(struct mem_hog_test *test)
{
    struct vk *vk = &test->vk;

    for (uint32_t i = 0; i < test->cpu.count; i++)
        free(test->cpu.bufs[i]);
    free(test->cpu.bufs);

    for (uint32_t i = 0; i < test->gpu.count; i++)
        vk_destroy_buffer(vk, test->gpu.bufs[i]);
    free(test->gpu.bufs);

    vk_cleanup(vk);
}

static int
mem_hog_test_thread(void *arg)
{
    struct mem_hog_test *test = arg;

    while (!atomic_load(&test->stop)) {
        for (uint32_t i = 0; i < test->cpu.count; i++) {
            for (uint32_t j = 0; j < test->cpu.page_count; j++)
                memset(test->cpu.bufs[i] + test->cpu.page_size * j, 0x37, 64);
        }

        if (test->cpu.sleep)
            u_sleep(test->cpu.sleep);
    }

    return 0;
}
static void
mem_hog_test_draw(struct mem_hog_test *test)
{
    struct vk *vk = &test->vk;

    if (test->gpu.count) {
        const float gpu_mb = test->gpu.size / 1024.0f / 1024.0f;
        const float total_gpu_gb = gpu_mb * test->gpu.count / 1024.0f;
        vk_log("buf size %.1fMiB, buf count %u, total buf size %.1fGiB", gpu_mb, test->gpu.count,
               total_gpu_gb);
    }

    if (test->cpu.count) {
        const float cpu_mb = test->cpu.size / 1024.0f / 1024.0f;
        const float total_cpu_gb = cpu_mb * test->cpu.count / 1024.0f;
        vk_log("sys size %.1fMiB, sys count %u, total sys size %.1fGiB", cpu_mb, test->cpu.count,
               total_cpu_gb);

        if (thrd_create(&test->thread, mem_hog_test_thread, test) != thrd_success)
            vk_die("failed to create thread");
    }

    while (true) {
        VkCommandBuffer cmd = vk_begin_cmd(vk, false);

        for (uint32_t i = 0; i < test->gpu.count; i++)
            vk->CmdFillBuffer(cmd, test->gpu.bufs[i]->buf, 0, 64, 0x37);

        vk_end_cmd(vk);

        if (test->gpu.sleep)
            u_sleep(test->gpu.sleep);
    }

    if (test->cpu.count) {
        atomic_store(&test->stop, true);
        if (thrd_join(test->thread, NULL) != thrd_success)
            vk_die("failed to join thread");
    }
}

int
main(int argc, char **argv)
{
    struct mem_hog_test test = {
        .gpu = {
            .size = 1ull * 1024 * 1024,
            .count = 1024,
            .sleep = 10,
        },
        .cpu = {
            .size = 1ull * 1024 * 1024,
            .count = 1024,
            .sleep = 5,
        },
    };

    if (argc > 1) {
        test.gpu.count = atoi(argv[1]);
        test.cpu.count = argc > 2 ? (uint32_t)atoi(argv[2]) : test.gpu.count;
    }

    mem_hog_test_init(&test);
    mem_hog_test_draw(&test);
    mem_hog_test_cleanup(&test);

    return 0;
}
