/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static const uint32_t bench_buffer_test_cs[] = {
#include "bench_buffer_test.comp.inc"
};

struct bench_buffer_test {
    VkDeviceSize size;
    uint32_t loop;

    uint32_t cs_local_size;
    uint32_t cs_elem_size;

    struct vk vk;
    struct vk_stopwatch *stopwatch;
};

static void
bench_buffer_test_init(struct bench_buffer_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);

    test->stopwatch = vk_create_stopwatch(vk, 2);
}

static void
bench_buffer_test_cleanup(struct bench_buffer_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_stopwatch(vk, test->stopwatch);
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
    memset(src, 0x7f, test->size);
    memcpy(dst, src, test->size);

    const uint64_t begin = u_now();
    for (uint32_t i = 0; i < test->loop; i++)
        memcpy(dst, src, test->size);
    const uint64_t end = u_now();

    return end - begin;
}

static uint64_t
bench_buffer_test_fill_buffer(struct bench_buffer_test *test, struct vk_buffer *buf)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);
    vk->CmdFillBuffer(cmd, buf->buf, 0, test->size, 0x7f7f7f7f);
    vk_end_cmd(vk);
    vk_wait(vk);

    cmd = vk_begin_cmd(vk, false);
    vk_write_stopwatch(vk, test->stopwatch, cmd);
    for (uint32_t i = 0; i < test->loop; i++)
        vk->CmdFillBuffer(cmd, buf->buf, 0, test->size, 0x7f7f7f7f);
    vk_write_stopwatch(vk, test->stopwatch, cmd);
    vk_end_cmd(vk);
    vk_wait(vk);

    const uint64_t dur = vk_read_stopwatch(vk, test->stopwatch, 0);
    vk_reset_stopwatch(vk, test->stopwatch);

    return dur;
}

static uint64_t
bench_buffer_test_copy_buffer(struct bench_buffer_test *test,
                              struct vk_buffer *dst,
                              struct vk_buffer *src)
{
    struct vk *vk = &test->vk;

    const VkBufferCopy copy = {
        .size = test->size,
    };

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);
    vk->CmdCopyBuffer(cmd, src->buf, dst->buf, 1, &copy);
    vk_end_cmd(vk);
    vk_wait(vk);

    cmd = vk_begin_cmd(vk, false);
    vk_write_stopwatch(vk, test->stopwatch, cmd);
    for (uint32_t i = 0; i < test->loop; i++)
        vk->CmdCopyBuffer(cmd, src->buf, dst->buf, 1, &copy);
    vk_write_stopwatch(vk, test->stopwatch, cmd);
    vk_end_cmd(vk);
    vk_wait(vk);

    const uint64_t dur = vk_read_stopwatch(vk, test->stopwatch, 0);
    vk_reset_stopwatch(vk, test->stopwatch);

    return dur;
}

static uint64_t
bench_buffer_test_dispatch(struct bench_buffer_test *test,
                           struct vk_buffer *dst,
                           struct vk_buffer *src)
{
    struct vk *vk = &test->vk;
    struct vk_pipeline *pipeline;
    struct vk_descriptor_set *set;

    {
        pipeline = vk_create_pipeline(vk);

        vk_add_pipeline_shader(vk, pipeline, VK_SHADER_STAGE_COMPUTE_BIT, bench_buffer_test_cs,
                               sizeof(bench_buffer_test_cs));

        const VkDescriptorSetLayoutBinding bindings[] = {
            [0] = {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            },
            [1] = {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            },
        };
        const VkDescriptorSetLayoutCreateInfo set_layout_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = ARRAY_SIZE(bindings),
            .pBindings = bindings,
        };
        vk_add_pipeline_set_layout_from_info(vk, pipeline, &set_layout_info);

        vk_setup_pipeline(vk, pipeline, NULL);
        vk_compile_pipeline(vk, pipeline);
    }

    {
        set = vk_create_descriptor_set(vk, pipeline->set_layouts[0]);

        const VkWriteDescriptorSet write_infos[] = {
            [0] = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = set->set,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &(VkDescriptorBufferInfo){
                    .buffer = dst->buf,
                    .range = test->size,
                },
            },
            [1] = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = set->set,
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &(VkDescriptorBufferInfo){
                    .buffer = src->buf,
                    .range = test->size,
                },
            },
        };
        vk->UpdateDescriptorSets(vk->dev, ARRAY_SIZE(write_infos), write_infos, 0, NULL);
    }

    const uint64_t grid_size = (uint64_t)sqrt(test->size / test->cs_elem_size);
    const uint32_t group_count = grid_size / test->cs_local_size;
    assert(grid_size * grid_size * test->cs_elem_size == test->size);
    assert(group_count * test->cs_local_size == grid_size);

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);
    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);
    vk->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline_layout, 0,
                              1, &set->set, 0, NULL);
    vk->CmdDispatch(cmd, group_count, group_count, 1);
    vk_end_cmd(vk);
    vk_wait(vk);

    cmd = vk_begin_cmd(vk, false);
    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);
    vk->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline_layout, 0,
                              1, &set->set, 0, NULL);
    vk_write_stopwatch(vk, test->stopwatch, cmd);
    for (uint32_t i = 0; i < test->loop; i++)
        vk->CmdDispatch(cmd, group_count, group_count, 1);
    vk_write_stopwatch(vk, test->stopwatch, cmd);
    vk_end_cmd(vk);
    vk_wait(vk);

    vk_destroy_pipeline(vk, pipeline);
    vk_destroy_descriptor_set(vk, set);

    const uint64_t dur = vk_read_stopwatch(vk, test->stopwatch, 0);
    vk_reset_stopwatch(vk, test->stopwatch);

    return dur;
}

static void
bench_buffer_test_draw_malloc(struct bench_buffer_test *test)
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
bench_buffer_test_draw_mt(struct bench_buffer_test *test, uint32_t mt_idx)
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
bench_buffer_test_draw_xfer(struct bench_buffer_test *test)
{
    struct vk *vk = &test->vk;
    char desc[64];

    const VkBufferUsageFlags usage =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const uint32_t mt_mask = vk_get_buffer_mt_mask(vk, 0, test->size, usage);

    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        if (!(mt_mask & (1 << i)))
            continue;

        struct vk_buffer *buf = vk_create_buffer_with_mt(vk, 0, test->size, usage, i);

        const uint64_t dur = bench_buffer_test_fill_buffer(test, buf);

        vk_destroy_buffer(vk, buf);

        vk_log("%s: vkCmdFillBuffer: %d MB/s", bench_buffer_test_describe_mt(test, i, desc),
               bench_buffer_test_calc_throughput_mb(test, dur));
    }

    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        if (!(mt_mask & (1 << i)))
            continue;

        struct vk_buffer *dst = vk_create_buffer_with_mt(vk, 0, test->size, usage, i);
        struct vk_buffer *src = vk_create_buffer_with_mt(vk, 0, test->size, usage, i);

        const uint64_t dur = bench_buffer_test_copy_buffer(test, dst, src);

        vk_destroy_buffer(vk, dst);
        vk_destroy_buffer(vk, src);

        vk_log("%s: vkCmdCopyBuffer: %d MB/s", bench_buffer_test_describe_mt(test, i, desc),
               bench_buffer_test_calc_throughput_mb(test, dur));
    }
}

static void
bench_buffer_test_draw_ssbo(struct bench_buffer_test *test)
{
    struct vk *vk = &test->vk;
    char desc[64];

    const VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const uint32_t mt_mask = vk_get_buffer_mt_mask(vk, 0, test->size, usage);

    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        if (!(mt_mask & (1 << i)))
            continue;

        struct vk_buffer *dst = vk_create_buffer_with_mt(vk, 0, test->size, usage, i);
        struct vk_buffer *src = vk_create_buffer_with_mt(vk, 0, test->size, usage, i);

        const uint64_t dur = bench_buffer_test_dispatch(test, dst, src);

        vk_destroy_buffer(vk, dst);
        vk_destroy_buffer(vk, src);

        vk_log("%s: SSBO: %d MB/s", bench_buffer_test_describe_mt(test, i, desc),
               bench_buffer_test_calc_throughput_mb(test, dur));
    }
}

static void
bench_buffer_test_draw(struct bench_buffer_test *test)
{
    struct vk *vk = &test->vk;

    bench_buffer_test_draw_malloc(test);

    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++)
        bench_buffer_test_draw_mt(test, i);

    bench_buffer_test_draw_xfer(test);
    bench_buffer_test_draw_ssbo(test);
}

int
main(int argc, char **argv)
{
    struct bench_buffer_test test = {
        .size = 64 * 1024 * 1024,
        .loop = 32,

        .cs_local_size = 8,
        .cs_elem_size = sizeof(uint32_t[4]),
    };

    bench_buffer_test_init(&test);
    bench_buffer_test_draw(&test);
    bench_buffer_test_cleanup(&test);

    return 0;
}
