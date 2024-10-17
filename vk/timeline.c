/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

struct timeline_test {
    uint64_t value;

    struct vk vk;

    struct vk_semaphore *sem;
};

static void
timeline_test_init(struct timeline_test *test)
{
    struct vk *vk = &test->vk;
    const struct vk_init_params params = {
        .api_version = VK_API_VERSION_1_3,
        .enable_all_features = true,
    };

    vk_init(vk, &params);

    test->sem = vk_create_semaphore(vk, VK_SEMAPHORE_TYPE_TIMELINE);
}

static void
timeline_test_cleanup(struct timeline_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_semaphore(vk, test->sem);
    vk_cleanup(vk);
}

static void
timeline_test_draw(struct timeline_test *test)
{
    struct vk *vk = &test->vk;

    const VkTimelineSemaphoreSubmitInfo sem_info = {
        .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues = &test->value,
    };
    const VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = &sem_info,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &test->sem->sem,
    };

    vk_log("before submit: %d", (int)vk_get_semaphore_counter_value(vk, test->sem));

    vk->result = vk->QueueSubmit(vk->queue, 1, &submit_info, VK_NULL_HANDLE);
    vk_check(vk, "failed to submit");

    vk_log("after submit: %d", (int)vk_get_semaphore_counter_value(vk, test->sem));

    const uint32_t ms = 5;
    u_sleep(ms);
    vk_log("after %dms: %d", ms, (int)vk_get_semaphore_counter_value(vk, test->sem));

    vk_wait(vk);

    vk_log("after wait: %d", (int)vk_get_semaphore_counter_value(vk, test->sem));
}

int
main(void)
{
    struct timeline_test test = {
        .value = 42,
    };

    timeline_test_init(&test);
    timeline_test_draw(&test);
    timeline_test_cleanup(&test);

    return 0;
}
