/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

struct sparse_buffer_test {
    VkDeviceSize buf_size;
    uint32_t fill_val;

    struct vk vk;

    VkBuffer buf;
    VkDeviceMemory mem;

    VkDeviceSize block_size;
    VkDeviceSize offsets[4];
};

static void
sparse_buffer_test_init_buffer(struct sparse_buffer_test *test)
{
    struct vk *vk = &test->vk;

    if (!vk->features.features.sparseResidencyBuffer)
        vk_die("no sparseResidencyBuffer");
    if (!vk->props.properties.sparseProperties.residencyNonResidentStrict)
        vk_log("residencyNonResidentStrict is not supported");

    if (test->buf_size > vk->props.properties.limits.sparseAddressSpaceSize)
        vk_die("buf_size exceeds sparseAddressSpaceSize");
    if (test->buf_size > vk->vulkan_13_props.maxBufferSize)
        vk_die("buf_size exceeds maxBufferSize");

    const VkBufferUsageFlags2CreateInfo usage_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO,
        .usage = VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
    };
    const VkBufferCreateInfo buf_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = &usage_info,
        .flags = VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT,
        .size = test->buf_size,
    };
    vk->result = vk->CreateBuffer(vk->dev, &buf_info, NULL, &test->buf);
    vk_check(vk, "failed to create sparse buffer");

    const VkBufferMemoryRequirementsInfo2 reqs_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
        .buffer = test->buf,
    };
    VkMemoryRequirements2 reqs2 = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
    };
    vk->GetBufferMemoryRequirements2(vk->dev, &reqs_info, &reqs2);
    const VkMemoryRequirements reqs = reqs2.memoryRequirements;

    test->block_size = reqs.alignment;
    const VkDeviceSize aligned_size = test->buf_size & ~(test->block_size * 2 - 1);
    if (aligned_size < test->block_size * ARRAY_SIZE(test->offsets))
        vk_die("bad buf_size %" PRIu64 " for block_size %" PRIu64, test->buf_size,
               test->block_size);

    /* 3 mapped areas and 1 unmapped area */
    const VkDeviceSize mem_size = test->block_size * (ARRAY_SIZE(test->offsets) - 1);
    const uint32_t mt_idx = (uint32_t)(ffs(reqs.memoryTypeBits) - 1);
    test->mem = vk_alloc_memory(vk, mem_size, mt_idx);

    test->offsets[0] = 0;
    test->offsets[1] = aligned_size / 2 - test->block_size;
    test->offsets[2] = aligned_size / 2;
    test->offsets[3] = aligned_size - test->block_size;

    VkSparseMemoryBind binds[ARRAY_SIZE(test->offsets) - 1];
    uint32_t bind_count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(test->offsets); i++) {
        if (i == 1)
            continue;

        const VkDeviceSize mem_offset = test->block_size * bind_count;
        binds[bind_count++] = (VkSparseMemoryBind){
            .resourceOffset = test->offsets[i],
            .size = test->block_size,
            .memory = test->mem,
            .memoryOffset = mem_offset,
        };
    }

    const VkSparseBufferMemoryBindInfo buf_bind_info = {
        .buffer = test->buf,
        .bindCount = bind_count,
        .pBinds = binds,
    };
    const VkBindSparseInfo bind_info = {
        .sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,
        .bufferBindCount = 1,
        .pBufferBinds = &buf_bind_info,
    };
    vk->result = vk->QueueBindSparse(vk->queue, 1, &bind_info, VK_NULL_HANDLE);
    vk_check(vk, "failed to bind sparse buffer");
    vk_wait(vk);
}

static void
sparse_buffer_test_init(struct sparse_buffer_test *test)
{
    struct vk *vk = &test->vk;
    const struct vk_init_params params = {
        .require_sparse = true,
    };

    vk_init(vk, &params);
    sparse_buffer_test_init_buffer(test);
}

static void
sparse_buffer_test_cleanup(struct sparse_buffer_test *test)
{
    struct vk *vk = &test->vk;

    vk->DestroyBuffer(vk->dev, test->buf, NULL);
    vk->FreeMemory(vk->dev, test->mem, NULL);

    vk_cleanup(vk);
}

static void
sparse_buffer_test_validate(struct sparse_buffer_test *test)
{
    struct vk *vk = &test->vk;

    struct vk_buffer *staging = vk_create_buffer(
        vk, 0, test->block_size * ARRAY_SIZE(test->offsets), VK_BUFFER_USAGE_2_TRANSFER_DST_BIT);
    memset(staging->mem_ptr, 0xaa, staging->info.size);

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    VkBufferCopy2 regions[ARRAY_SIZE(test->offsets)];
    for (uint32_t i = 0; i < ARRAY_SIZE(test->offsets); i++) {
        regions[i] = (VkBufferCopy2){
            .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
            .srcOffset = test->offsets[i],
            .dstOffset = test->block_size * i,
            .size = test->block_size,
        };
    }
    const VkCopyBufferInfo2 copy_info = {
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer = test->buf,
        .dstBuffer = staging->buf,
        .regionCount = ARRAY_SIZE(regions),
        .pRegions = regions,
    };
    vk->CmdCopyBuffer2(cmd, &copy_info);

    const VkBufferMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
        .buffer = staging->buf,
        .offset = 0,
        .size = VK_WHOLE_SIZE,
    };
    const VkDependencyInfo dep = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &barrier,
    };
    vk->CmdPipelineBarrier2(cmd, &dep);

    vk_end_cmd(vk);
    vk_wait(vk);

    for (uint32_t i = 0; i < ARRAY_SIZE(test->offsets); i++) {
        const uint32_t expected = i != 1 ? test->fill_val : 0;
        const uint32_t *words =
            (const uint32_t *)((const uint8_t *)staging->mem_ptr + test->block_size * i);
        const uint32_t count = test->block_size / sizeof(*words);

        for (uint32_t j = 0; j < count; j++) {
            if (words[j] != expected)
                vk_die("area %u word %u is 0x%x, expected 0x%x", i, j, words[j], expected);
        }
    }

    vk_destroy_buffer(vk, staging);

    vk_log("all %zu areas validated", ARRAY_SIZE(test->offsets));
}

static void
sparse_buffer_test_draw(struct sparse_buffer_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    vk->CmdFillBuffer(cmd, test->buf, test->offsets[0], test->block_size, test->fill_val);

    const VkBufferMemoryBarrier2 fill_barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .buffer = test->buf,
        .offset = test->offsets[0],
        .size = test->block_size,
    };
    const VkDependencyInfo fill_dep = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &fill_barrier,
    };
    vk->CmdPipelineBarrier2(cmd, &fill_dep);

    VkBufferCopy2 regions[ARRAY_SIZE(test->offsets) - 1];
    for (uint32_t i = 0; i < ARRAY_SIZE(regions); i++) {
        regions[i] = (VkBufferCopy2){
            .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
            .srcOffset = test->offsets[0],
            .dstOffset = test->offsets[i + 1],
            .size = test->block_size,
        };
    }
    const VkCopyBufferInfo2 copy_info = {
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer = test->buf,
        .dstBuffer = test->buf,
        .regionCount = ARRAY_SIZE(regions),
        .pRegions = regions,
    };
    vk->CmdCopyBuffer2(cmd, &copy_info);

    vk_end_cmd(vk);
    vk_wait(vk);

    sparse_buffer_test_validate(test);
}

int
main(void)
{
    struct sparse_buffer_test test = {
        .buf_size = 1ull * 1024 * 1024 * 1024,
        .fill_val = 0x12345678,
    };

    sparse_buffer_test_init(&test);
    sparse_buffer_test_draw(&test);
    sparse_buffer_test_cleanup(&test);

    return 0;
}
