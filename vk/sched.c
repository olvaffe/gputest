/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

#include <linux/prctl.h>
#include <sched.h>
#include <stdatomic.h>
#include <sys/prctl.h>
#include <threads.h>

static const uint32_t sched_test_cs[] = {
#include "sched_test.comp.inc"
};

struct sched_test_push_consts {
    uint32_t loop;
};

struct sched_test {
    /* cpu */
    bool cpu_fifo;
    uint32_t cpu_loop;
    uint32_t cpu_pre_busy;
    uint32_t cpu_post_sleep;
    /* gpu */
    uint32_t group_count;
    uint32_t local_size;
    uint32_t type_size;
    uint32_t loop;

    struct vk vk;

    struct vk_buffer *src;
    struct vk_buffer *dst;
    struct vk_buffer *weight;

    struct vk_pipeline *pipeline;
    struct vk_descriptor_set *set;

    thrd_t *threads;
    uint32_t thread_count;
    atomic_bool stop;
};

static void
sched_test_set_fifo(void)
{
    const int policy = SCHED_FIFO;
    const int prio = sched_get_priority_min(policy);
    if (prio < 0)
        vk_die("failed to get max sched prio");

    const struct sched_param param = {
        .sched_priority = prio,
    };
    if (sched_setscheduler(0, SCHED_FIFO, &param))
        vk_die("failed to set sched");
}

static void
sched_test_busy_loop(uint32_t ms)
{
    const uint64_t end = u_now() + (uint64_t)ms * 1000 * 1000;
    while (u_now() < end)
        ;
}

static int
sched_test_thread(void *arg)
{
    struct sched_test *test = arg;

    prctl(PR_SET_NAME, "noise");

    if (test->cpu_fifo)
        sched_test_set_fifo();
    while (!atomic_load(&test->stop)) {
        sched_test_busy_loop(test->cpu_pre_busy);
        u_sleep(test->cpu_post_sleep);
    }

    return 0;
}

static void
sched_test_init_threads(struct sched_test *test)
{
    atomic_init(&test->stop, false);

    const int ret = sysconf(_SC_NPROCESSORS_ONLN);
    if (ret <= 0)
        vk_die("failed to get core count");

    if (test->cpu_fifo)
        test->thread_count = ret - 1;
    else
        test->thread_count = ret * 2;

    test->threads = malloc(sizeof(*test->threads) * test->thread_count);
    if (!test->threads)
        vk_die("failed to alloc threads");

    for (uint32_t i = 0; i < test->thread_count; i++) {
        if (thrd_create(&test->threads[i], sched_test_thread, test) != thrd_success)
            vk_die("failed to create thread");
    }
}

static void
sched_test_init_descriptor_set(struct sched_test *test)
{
    struct vk *vk = &test->vk;

    test->set = vk_create_descriptor_set(vk, test->pipeline->set_layouts[0]);

    vk_write_descriptor_set_buffer(vk, test->set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, test->dst,
                                   VK_WHOLE_SIZE);
}

static void
sched_test_init_pipeline(struct sched_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_COMPUTE_BIT, sched_test_cs,
                           sizeof(sched_test_cs));

    vk_add_pipeline_set_layout(vk, test->pipeline, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                               VK_SHADER_STAGE_COMPUTE_BIT, NULL);

    vk_set_pipeline_push_const(vk, test->pipeline, VK_SHADER_STAGE_COMPUTE_BIT,
                               sizeof(struct sched_test_push_consts));

    vk_setup_pipeline(vk, test->pipeline, NULL);
    vk_compile_pipeline(vk, test->pipeline);
}

static void
sched_test_init_buffer(struct sched_test *test)
{
    struct vk *vk = &test->vk;

    const VkDeviceSize buf_size = test->group_count * test->local_size * test->type_size;
    test->dst = vk_create_buffer(vk, 0, buf_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

static void
sched_test_init(struct sched_test *test)
{
    struct vk *vk = &test->vk;

    const char *dev_exts[] = { VK_KHR_GLOBAL_PRIORITY_EXTENSION_NAME };
    const struct vk_init_params params = {
        .high_priority = true,
        .dev_exts = dev_exts,
        .dev_ext_count = ARRAY_SIZE(dev_exts),
    };
    vk_init(vk, &params);

    sched_test_init_buffer(test);
    sched_test_init_pipeline(test);
    sched_test_init_descriptor_set(test);

    sched_test_init_threads(test);
}

static void
sched_test_cleanup(struct sched_test *test)
{
    struct vk *vk = &test->vk;

    atomic_store(&test->stop, true);
    for (uint32_t i = 0; i < test->thread_count; i++) {
        if (thrd_join(test->threads[i], NULL) != thrd_success)
            vk_die("failed to join threads");
    }
    free(test->threads);

    vk_destroy_descriptor_set(vk, test->set);
    vk_destroy_pipeline(vk, test->pipeline);
    vk_destroy_buffer(vk, test->dst);

    vk_cleanup(vk);
}

static void
sched_test_dispatch_once(struct sched_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, test->pipeline->pipeline);
    vk->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                              test->pipeline->pipeline_layout, 0, 1, &test->set->set, 0, NULL);

    const struct sched_test_push_consts consts = {
        .loop = test->loop,
    };
    vk->CmdPushConstants(cmd, test->pipeline->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(consts), &consts);

    vk->CmdDispatch(cmd, test->group_count, 1, 1);

    vk_end_cmd(vk);
    vk_wait(vk);
}

static void
sched_test_dispatch(struct sched_test *test)
{
    if (test->cpu_fifo)
        sched_test_set_fifo();

    for (uint32_t i = 0; i < test->cpu_loop; i++) {
        sched_test_busy_loop(test->cpu_pre_busy);
        sched_test_dispatch_once(test);
        u_sleep(test->cpu_post_sleep);
    }
}

int
main(void)
{
    struct sched_test test = {
        .cpu_fifo = false,
        .cpu_loop = 300,
        .cpu_pre_busy = 3,
        .cpu_post_sleep = 2,
        .group_count = 64,
        .local_size = 64,
        .type_size = 4,
        .loop = 50000,
    };

    sched_test_init(&test);
    sched_test_dispatch(&test);
    sched_test_cleanup(&test);

    return 0;
}
