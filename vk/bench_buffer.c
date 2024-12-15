/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

struct bench_buffer_test {
    VkDeviceSize size;
    uint32_t loop;

    struct vk vk;
};

static void
bench_buffer_test_init(struct bench_buffer_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);
}

static void
bench_buffer_test_cleanup(struct bench_buffer_test *test)
{
    struct vk *vk = &test->vk;
    vk_cleanup(vk);
}

static const char *
bench_buffer_test_describe_mt(struct bench_buffer_test *test,
                              uint32_t mt_idx,
                              char desc[static 64])
{
    struct vk *vk = &test->vk;
    const VkMemoryType *mt = &vk->mem_props.memoryTypes[mt_idx];
    const bool mt_local = mt->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    const bool mt_coherent = mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    const bool mt_cached = mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

    snprintf(desc, 64, "mt %d (%s%s%s)", mt_idx, mt_local ? "Lo" : "..",
             mt_coherent ? "Co" : "..", mt_cached ? "Ca" : "..");

    return desc;
}

static uint64_t
bench_buffer_test_calc_throughput(struct bench_buffer_test *test, uint64_t dur)
{
    const uint64_t ns_per_s = 1000000000;
    return test->size * test->loop * ns_per_s / dur;
}

static uint32_t
bench_buffer_test_calc_throughput_mb(struct bench_buffer_test *test, uint64_t dur)
{
    return bench_buffer_test_calc_throughput(test, dur) / 1024 / 1024;
}

static uint64_t
bench_buffer_test_memset(struct bench_buffer_test *test, void *buf)
{
    memset(buf, 0x7f, test->size);

    const uint64_t begin = u_now();
    for (uint32_t i = 0; i < test->loop; i++)
        memset(buf, 0x7f, test->size);
    const uint64_t end = u_now();

    return end - begin;
}

static uint64_t
bench_buffer_test_memcpy(struct bench_buffer_test *test, void *dst, void *src)
{
    memcpy(dst, src, test->size);

    const uint64_t begin = u_now();
    for (uint32_t i = 0; i < test->loop; i++)
        memcpy(dst, src, test->size);
    const uint64_t end = u_now();

    return end - begin;
}

static uint64_t
bench_buffer_test_fill_buffer(struct bench_buffer_test *test, VkBuffer buf)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);
    vk->CmdFillBuffer(cmd, buf, 0, test->size, 0x7f7f7f7f);
    vk_end_cmd(vk);
    vk_wait(vk);

    struct vk_stopwatch *stopwatch = vk_create_stopwatch(vk, 2);

    cmd = vk_begin_cmd(vk, false);
    vk_write_stopwatch(vk, stopwatch, cmd);
    for (uint32_t i = 0; i < test->loop; i++)
        vk->CmdFillBuffer(cmd, buf, 0, test->size, 0x7f7f7f7f);
    vk_write_stopwatch(vk, stopwatch, cmd);
    vk_end_cmd(vk);
    vk_wait(vk);

    const uint64_t dur = vk_read_stopwatch(vk, stopwatch, 0);
    vk_destroy_stopwatch(vk, stopwatch);

    return dur;
}

static uint64_t
bench_buffer_test_copy_buffer(struct bench_buffer_test *test, VkBuffer dst, VkBuffer src)
{
    struct vk *vk = &test->vk;

    const VkBufferCopy copy = {
        .size = test->size,
    };

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);
    vk->CmdCopyBuffer(cmd, src, dst, 1, &copy);
    vk_end_cmd(vk);
    vk_wait(vk);

    struct vk_stopwatch *stopwatch = vk_create_stopwatch(vk, 2);

    cmd = vk_begin_cmd(vk, false);
    vk_write_stopwatch(vk, stopwatch, cmd);
    for (uint32_t i = 0; i < test->loop; i++)
        vk->CmdCopyBuffer(cmd, src, dst, 1, &copy);
    vk_write_stopwatch(vk, stopwatch, cmd);
    vk_end_cmd(vk);
    vk_wait(vk);

    const uint64_t dur = vk_read_stopwatch(vk, stopwatch, 0);
    vk_destroy_stopwatch(vk, stopwatch);

    return dur;
}

static void
bench_buffer_test_malloc(struct bench_buffer_test *test)
{
    {
        void *mem = malloc(test->size);
        if (!mem)
            vk_die("failed to malloc");

        uint64_t dur = bench_buffer_test_memset(test, mem);

        free(mem);

        vk_log("malloc: memset: %d MB/s", bench_buffer_test_calc_throughput_mb(test, dur));
    }

    {
        void *dst = malloc(test->size);
        void *src = malloc(test->size);
        if (!dst || !src)
            vk_die("failed to malloc");

        uint64_t dur = bench_buffer_test_memcpy(test, dst, src);

        free(dst);
        free(src);

        vk_log("malloc: memcpy: %d MB/s", bench_buffer_test_calc_throughput_mb(test, dur));
    }
}

static void
bench_buffer_test_mt(struct bench_buffer_test *test, uint32_t mt_idx)
{
    struct vk *vk = &test->vk;

    const VkMemoryType *mt = &vk->mem_props.memoryTypes[mt_idx];
    if (!(mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
        return;

    char desc[64];
    bench_buffer_test_describe_mt(test, mt_idx, desc);

    {
        VkDeviceMemory mem = vk_alloc_memory(vk, test->size, mt_idx);

        void *mem_ptr;
        vk->result = vk->MapMemory(vk->dev, mem, 0, test->size, 0, &mem_ptr);
        vk_check(vk, "failed to map memory");

        uint64_t dur = bench_buffer_test_memset(test, mem_ptr);

        vk->FreeMemory(vk->dev, mem, NULL);

        vk_log("%s: memset: %d MB/s", desc, bench_buffer_test_calc_throughput_mb(test, dur));
    }

    {
        VkDeviceMemory dst = vk_alloc_memory(vk, test->size, mt_idx);
        VkDeviceMemory src = vk_alloc_memory(vk, test->size, mt_idx);

        void *dst_ptr;
        void *src_ptr;
        vk->result = vk->MapMemory(vk->dev, dst, 0, test->size, 0, &dst_ptr);
        vk_check(vk, "failed to map memory");
        vk->result = vk->MapMemory(vk->dev, src, 0, test->size, 0, &src_ptr);
        vk_check(vk, "failed to map memory");

        uint64_t dur = bench_buffer_test_memcpy(test, dst_ptr, src_ptr);

        vk->FreeMemory(vk->dev, dst, NULL);
        vk->FreeMemory(vk->dev, src, NULL);

        vk_log("%s: memcpy: %d MB/s", desc, bench_buffer_test_calc_throughput_mb(test, dur));
    }
}

static void
bench_buffer_test_xfer(struct bench_buffer_test *test)
{
    struct vk *vk = &test->vk;
    char desc[64];

    const VkBufferCreateInfo test_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = test->size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    VkBuffer test_buf;
    vk->result = vk->CreateBuffer(vk->dev, &test_info, NULL, &test_buf);
    vk_check(vk, "failed to create buffer");

    VkMemoryRequirements test_reqs;
    vk->GetBufferMemoryRequirements(vk->dev, test_buf, &test_reqs);

    vk->DestroyBuffer(vk->dev, test_buf, NULL);

    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        if (!(test_reqs.memoryTypeBits & (1 << i)))
            continue;

        VkBuffer buf;
        vk->result = vk->CreateBuffer(vk->dev, &test_info, NULL, &buf);
        vk_check(vk, "failed to create buffer");

        VkDeviceMemory mem = vk_alloc_memory(vk, test_reqs.size, i);

        vk->result = vk->BindBufferMemory(vk->dev, buf, mem, 0);
        vk_check(vk, "failed to bind buffer memory");

        const uint64_t dur = bench_buffer_test_fill_buffer(test, buf);

        vk->FreeMemory(vk->dev, mem, NULL);
        vk->DestroyBuffer(vk->dev, buf, NULL);

        vk_log("%s: vkCmdFillBuffer: %d MB/s", bench_buffer_test_describe_mt(test, i, desc),
               bench_buffer_test_calc_throughput_mb(test, dur));
    }

    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        if (!(test_reqs.memoryTypeBits & (1 << i)))
            continue;

        VkBuffer dst;
        VkBuffer src;
        vk->result = vk->CreateBuffer(vk->dev, &test_info, NULL, &dst);
        vk_check(vk, "failed to create buffer");
        vk->result = vk->CreateBuffer(vk->dev, &test_info, NULL, &src);
        vk_check(vk, "failed to create buffer");

        VkDeviceMemory dst_mem = vk_alloc_memory(vk, test_reqs.size, i);
        VkDeviceMemory src_mem = vk_alloc_memory(vk, test_reqs.size, i);

        vk->result = vk->BindBufferMemory(vk->dev, dst, dst_mem, 0);
        vk_check(vk, "failed to bind buffer memory");
        vk->result = vk->BindBufferMemory(vk->dev, src, src_mem, 0);
        vk_check(vk, "failed to bind buffer memory");

        const uint64_t dur = bench_buffer_test_copy_buffer(test, dst, src);

        vk->FreeMemory(vk->dev, dst_mem, NULL);
        vk->FreeMemory(vk->dev, src_mem, NULL);
        vk->DestroyBuffer(vk->dev, dst, NULL);
        vk->DestroyBuffer(vk->dev, src, NULL);

        vk_log("%s: vkCmdCopyBuffer: %d MB/s", bench_buffer_test_describe_mt(test, i, desc),
               bench_buffer_test_calc_throughput_mb(test, dur));
    }
}

static void
bench_buffer_test_draw(struct bench_buffer_test *test)
{
    struct vk *vk = &test->vk;

    bench_buffer_test_malloc(test);

    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++)
        bench_buffer_test_mt(test, i);

    bench_buffer_test_xfer(test);
}

int
main(int argc, char **argv)
{
    struct bench_buffer_test test = {
        .size = 64 * 1024 * 1024,
        .loop = 32,
    };

    bench_buffer_test_init(&test);
    bench_buffer_test_draw(&test);
    bench_buffer_test_cleanup(&test);

    return 0;
}