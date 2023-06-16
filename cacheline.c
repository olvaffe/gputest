/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

struct cacheline_test {
    uint32_t dword_count;

    struct vk vk;
    struct vk_buffer *buf;
    struct vk_event *gpu_done;
    struct vk_event *cpu_done;
};

static void
cacheline_test_init_buf(struct cacheline_test *test)
{
    struct vk *vk = &test->vk;

    const VkDeviceSize size = test->dword_count * 4;
    test->buf = vk_create_buffer(vk, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
}

static void
cacheline_test_init(struct cacheline_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);
    cacheline_test_init_buf(test);
    test->gpu_done = vk_create_event(vk);
    test->cpu_done = vk_create_event(vk);
}

static void
cacheline_test_cleanup(struct cacheline_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_buffer(vk, test->buf);
    vk_destroy_event(vk, test->gpu_done);
    vk_destroy_event(vk, test->cpu_done);

    vk_cleanup(vk);
}

static void
cacheline_test_draw(struct cacheline_test *test)
{
    struct vk *vk = &test->vk;

    /* We do
     *
     *   1. cpu memsets the buffer to 0
     *   2. gpu writes 1 to dword 1 and 2
     *   3. cpu writes 2 to dword 2
     *   4. cpu writes 3 to dword 3
     *   5. gpu flushes its cache
     *
     * in order and expect step 3 and 4 to have no effect because of step 5.
     *
     * Note that this is written with anv in mind, not a general test case.
     */
    assert(test->dword_count >= 4);
    volatile uint32_t *dwords = test->buf->mem_ptr;

    /* step 1 */
    memset(test->buf->mem_ptr, 0, test->dword_count * 4);

    /* step 2: build a command to write dword 1 and 2 */
    VkCommandBuffer cmd = vk_begin_cmd(vk);

    const VkBufferMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .buffer = test->buf->buf,
        .offset = 4,
        .size = 8,
    };
    const VkEvent events[] = { test->gpu_done->event, test->cpu_done->event };

    vk->CmdFillBuffer(cmd, test->buf->buf, 4, 8, 1);
    vk->CmdSetEvent(cmd, test->gpu_done->event, VK_PIPELINE_STAGE_TRANSFER_BIT);
    vk->CmdWaitEvents(cmd, ARRAY_SIZE(events), events,
                      VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT,
                      VK_PIPELINE_STAGE_HOST_BIT, 0, NULL, 1, &barrier, 0, NULL);

    /* step 2: submit */
    vk_end_cmd(vk);
    /* step 2: wait */
    while (vk->GetEventStatus(vk->dev, test->gpu_done->event) != VK_EVENT_SET)
        vk_sleep(1);

    vk_log("after CmdFillBuffer but before VkBufferMemoryBarrier");
    for (uint32_t i = 0; i < 4; i++)
        vk_log("dword[%d] = %d", i, dwords[i]);

    /* step 3: this will be lost */
    dwords[2] = 2;
    /* step 4: this will be lost */
    dwords[3] = 3;

    vk_log("after host writes");
    for (uint32_t i = 0; i < 4; i++)
        vk_log("dword[%d] = %d", i, dwords[i]);

    /* step 4: execute the gpu barrier to flush the gpu cache */
    vk->SetEvent(vk->dev, test->cpu_done->event);
    vk_wait(vk);

    vk_log("after VkBufferMemoryBarrier");
    for (uint32_t i = 0; i < 4; i++)
        vk_log("dword[%d] = %d", i, dwords[i]);
}

int
main(void)
{
    struct cacheline_test test = {
        .dword_count = 16,
    };

    cacheline_test_init(&test);
    cacheline_test_draw(&test);
    cacheline_test_cleanup(&test);

    return 0;
}
