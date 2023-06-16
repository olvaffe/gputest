/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

struct buf_align_test {
    struct vk vk;

    VkDeviceSize mem_size;
    VkDeviceSize buf_size;
    VkBufferUsageFlags buf_usage;
    VkDeviceSize force_alignment;

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

#define ALIGN(OFFSET, ALIGN) (((OFFSET) + (ALIGN)-1) & ~((ALIGN)-1))

static void
buf_align_test_init(struct buf_align_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);

    /* allocate a page to be suballocated for VkBuffer */
    test->mem = vk_alloc_memory(vk, 4096, vk->buf_mt_index);
    test->mem_used = 0;
    vk->result = vk->MapMemory(vk->dev, test->mem, 0, test->mem_size, 0, &test->mem_ptr);
    vk_check(vk, "failed to map memory");

    const VkBufferCreateInfo buf_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = test->buf_size,
        .usage = test->buf_usage,
    };
    /* create a buffer to disturb the cacheline via gpu flush */
    vk->result = vk->CreateBuffer(vk->dev, &buf_info, NULL, &test->disturb);
    vk_check(vk, "failed to create buffer");

    /* create a buffer as the blit src */
    vk->result = vk->CreateBuffer(vk->dev, &buf_info, NULL, &test->src_buf);
    vk_check(vk, "failed to create buffer");

    VkMemoryRequirements reqs;
    vk->GetBufferMemoryRequirements(vk->dev, test->disturb, &reqs);
    if (!(reqs.memoryTypeBits & (1u << vk->buf_mt_index)))
        vk_die("failed to meet buf memory reqs: 0x%x", reqs.memoryTypeBits);
    vk_log("buffer memory alignment = %" PRIu64 "", reqs.alignment);

    VkDeviceSize mem_offset = 0;
    vk->result = vk->BindBufferMemory(vk->dev, test->disturb, test->mem, mem_offset);
    vk_check(vk, "failed to bind buffer memory");
    test->disturb_ptr = (void *)((uint8_t *)test->mem_ptr + mem_offset);
    vk_log("suballoc disturb of size=%" PRIu64 " at offset=%" PRIu64 "", reqs.size, mem_offset);

    mem_offset += reqs.size;
    mem_offset = ALIGN(mem_offset, reqs.alignment);
    /* additionally align the offset to suballoc if specified */
    if (test->force_alignment) {
        vk_log("force additional alignment = %" PRIu64 "", test->force_alignment);
        mem_offset = ALIGN(mem_offset, test->force_alignment);
    }
    vk->result = vk->BindBufferMemory(vk->dev, test->src_buf, test->mem, mem_offset);
    vk_check(vk, "failed to bind buffer memory");
    test->src_buf_ptr = (void *)((uint8_t *)test->mem_ptr + mem_offset);
    vk_log("suballoc src_buf of size=%" PRIu64 " at offset=%" PRIu64 "", reqs.size, mem_offset);

    test->buf_with_mem = vk_create_buffer(vk, test->buf_size, test->buf_usage);
    test->dst_buf = test->buf_with_mem->buf;
    test->dst_buf_ptr = test->buf_with_mem->mem_ptr;
    vk_log("allocate dst_buf of size=%" PRIu64 " from separate memory", reqs.size);

    test->gpu_done = vk_create_event(vk);
    test->cpu_done = vk_create_event(vk);
}

static void
buf_align_test_cleanup(struct buf_align_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_event(vk, test->cpu_done);
    vk_destroy_event(vk, test->gpu_done);

    vk_destroy_buffer(vk, test->buf_with_mem);

    vk->DestroyBuffer(vk->dev, test->src_buf, NULL);
    vk->DestroyBuffer(vk->dev, test->disturb, NULL);

    vk->UnmapMemory(vk->dev, test->mem);
    vk->FreeMemory(vk->dev, test->mem, NULL);

    vk_cleanup(vk);
}

static void
buf_align_test_draw(struct buf_align_test *test)
{
    struct vk *vk = &test->vk;

    /* Env
     *  - ToT MESA at 25c1f325d081f6182ee784dcb927d16b79136c66
     *  - CML and ADL
     *
     * Setup
     *  - buffer size is 4 (mem req size=16 alignment=16)
     *  - disturb buffer is used to affect cacheline with gpu cache flush
     *  - disturb and src_buf are suballocated from the same device memory
     *  - disturb is bound at offset = 0
     *  - src_buf is bound at offset = align(req.size, req.alignment)
     *  - dst_buf is bound with a separate device memory
     *
     * We do
     *   1. cpu memsets both memories to 0
     *   2. gpu writes 1 to disturb
     *   3. cpu writes 2 to src_buf
     *   4. gpu flushes its cache
     *   5. normally blit src_buf to dst_buf in a different submit
     *   6. check dst_buf blit result
     *
     * Result
     *   - No issues with CML while broken on ADL as below
     *   - Order is ensured, and (3) is lost because of (4), and (6) proves it.
     *   - Uncomment "//.force_alignment = 64," at the bottom can workaround ADL.
     */

    /* step 1 */
    memset(test->mem_ptr, 0, test->mem_size);
    memset(test->buf_with_mem->mem_ptr, 0, test->buf_with_mem->mem_size);

    /* step 2: build a command to write 1 to disturb */
    VkCommandBuffer cmd1 = vk_begin_cmd(vk);
    const VkBufferMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .buffer = test->disturb,
        .offset = 0,
        .size = 4,
    };
    vk->CmdFillBuffer(cmd1, test->disturb, 0, 4, 1);
    vk->CmdSetEvent(cmd1, test->gpu_done->event, VK_PIPELINE_STAGE_TRANSFER_BIT);
    vk->CmdWaitEvents(cmd1, 1, &test->cpu_done->event, VK_PIPELINE_STAGE_TRANSFER_BIT,
                      VK_PIPELINE_STAGE_HOST_BIT, 0, NULL, 1, &barrier, 0, NULL);
    vk_end_cmd(vk);
    while (vk->GetEventStatus(vk->dev, test->gpu_done->event) != VK_EVENT_SET)
        vk_sleep(1);

    vk_log("disturb: after CmdFillBuffer but before VkBufferMemoryBarrier");
    vk_log("disturb = %u", *test->disturb_ptr);
    vk_log("src_buf = %u", *test->src_buf_ptr);
    vk_log("dst_buf = %u", *test->dst_buf_ptr);

    /* step 3: host writes 2 to src_buf, which will be lost on ADL */
    *test->src_buf_ptr = 2;

    vk_log("src_buf: after host writes 2");
    vk_log("disturb = %u", *test->disturb_ptr);
    vk_log("src_buf = %u", *test->src_buf_ptr);
    vk_log("dst_buf = %u", *test->dst_buf_ptr);

    /* step 4: execute the gpu barrier to flush the gpu cache for disturb */
    vk->SetEvent(vk->dev, test->cpu_done->event);
    vk_wait(vk);

    vk_log("disturb: after VkBufferMemoryBarrier");
    vk_log("disturb = %u", *test->disturb_ptr);
    vk_log("src_buf = %u", *test->src_buf_ptr);
    vk_log("dst_buf = %u", *test->dst_buf_ptr);

    /* step 5: build a command to blit src_buf to dst_buf */
    VkCommandBuffer cmd2 = vk_begin_cmd(vk);
    const VkBufferMemoryBarrier src_buf_barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .buffer = test->src_buf,
        .offset = 0,
        .size = 4,
    };
    vk->CmdPipelineBarrier(cmd2, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                           NULL, 1, &src_buf_barrier, 0, NULL);
    const VkBufferCopy copy = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = 4,
    };
    vk->CmdCopyBuffer(cmd2, test->src_buf, test->dst_buf, 1, &copy);
    const VkBufferMemoryBarrier dst_buf_barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .buffer = test->dst_buf,
        .offset = 0,
        .size = 4,
    };
    vk->CmdPipelineBarrier(cmd2, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                           NULL, 1, &dst_buf_barrier, 0, NULL);
    vk_end_cmd(vk);
    vk_wait(vk);

    /* step 6: check dst_buf blit result */
    vk_log("dst_buf: after vkCmdCopyBuffer");
    vk_log("disturb = %u", *test->disturb_ptr);
    vk_log("src_buf = %u", *test->src_buf_ptr);
    vk_log("dst_buf = %u", *test->dst_buf_ptr);
}

int
main(void)
{
    struct buf_align_test test = {
        .mem_size = 4096,
        .buf_size = 4,
        .buf_usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        //.force_alignment = 64,
    };

    buf_align_test_init(&test);
    buf_align_test_draw(&test);
    buf_align_test_cleanup(&test);

    return 0;
}
