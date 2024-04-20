/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

/* This allocates an SSBO that is close to maxStorageBufferRange and verifies
 * that it can be written to.
 */

#include "vkutil.h"

static const uint32_t desc_buf_test_cs[] = {
#include "desc_buf_test.comp.inc"
};

struct desc_buf_test {
    uint32_t global_size;
    uint32_t local_size;
    VkFormat item_format;
    uint32_t item_size;
    uint32_t loop;

    struct vk vk;

    struct vk_buffer *buf;

    VkDeviceSize src_ubo_offset;
    VkDeviceSize src_ubo_size;
    VkDeviceSize src_ssbo_offset;
    VkDeviceSize src_ssbo_size;
    VkDeviceSize src_tbo_offset;
    VkDeviceSize src_tbo_size;
    VkBufferView src_tbo_view;
    VkDeviceSize src_ibo_offset;
    VkDeviceSize src_ibo_size;
    VkBufferView src_ibo_view;

    VkDeviceSize dst_ssbo_offset;
    VkDeviceSize dst_ssbo_size;
    VkDeviceSize dst_ibo_offset;
    VkDeviceSize dst_ibo_size;
    VkBufferView dst_ibo_view;

    struct vk_pipeline *pipeline;
    struct vk_descriptor_set *set;

    struct vk_stopwatch *stopwatch;
};

static void
desc_buf_test_init_descriptor_set(struct desc_buf_test *test)
{
    struct vk *vk = &test->vk;

    test->set = vk_create_descriptor_set(vk, test->pipeline->set_layouts[0]);

    const VkWriteDescriptorSet write_infos[] = {
        [0] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = test->set->set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &(VkDescriptorBufferInfo){
                .buffer = test->buf->buf,
                .offset = test->src_ubo_offset,
                .range = test->src_ubo_size,
            },
        },
        [1] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = test->set->set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &(VkDescriptorBufferInfo){
                .buffer = test->buf->buf,
                .offset = test->src_ssbo_offset,
                .range = test->src_ssbo_size,
            },
        },
        [2] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = test->set->set,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
            .pTexelBufferView = &test->src_tbo_view,
        },
        [3] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = test->set->set,
            .dstBinding = 3,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
            .pTexelBufferView = &test->src_ibo_view,
        },
        [4] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = test->set->set,
            .dstBinding = 4,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &(VkDescriptorBufferInfo){
                .buffer = test->buf->buf,
                .offset = test->dst_ssbo_offset,
                .range = test->dst_ssbo_size,
            },
        },
        [5] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = test->set->set,
            .dstBinding = 5,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
            .pTexelBufferView = &test->dst_ibo_view,
        },
    };
    vk->UpdateDescriptorSets(vk->dev, ARRAY_SIZE(write_infos), write_infos, 0, NULL);
}

static void
desc_buf_test_init_pipeline(struct desc_buf_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_COMPUTE_BIT, desc_buf_test_cs,
                           sizeof(desc_buf_test_cs));

    const VkDescriptorSetLayoutBinding bindings[] = {
        [0] = {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        [1] = {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        [2] = {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        [3] = {
            .binding = 3,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        [4] = {
            .binding = 4,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        [5] = {
            .binding = 5,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
    };
    const VkDescriptorSetLayoutCreateInfo set_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ARRAY_SIZE(bindings),
        .pBindings = bindings,
    };
    vk_add_pipeline_set_layout_from_info(vk, test->pipeline, &set_layout_info);

    vk_setup_pipeline(vk, test->pipeline, NULL);
    vk_compile_pipeline(vk, test->pipeline);
}

static void
desc_buf_test_init_buffer(struct desc_buf_test *test)
{
    struct vk *vk = &test->vk;

    const VkPhysicalDeviceLimits *limits = &vk->props.properties.limits;
    const VkDeviceSize buf_size = test->global_size * test->item_size;
    VkDeviceSize alloc_size = 0;

    if (test->global_size > limits->maxTexelBufferElements) {
        vk_die("test requires %u elements but the limit is %u", test->global_size,
               limits->maxTexelBufferElements);
    }
    if (buf_size > limits->maxUniformBufferRange) {
        vk_die("test requires ubo size %u but the limit is %u", (unsigned)buf_size,
               limits->maxUniformBufferRange);
    }
    if (buf_size > limits->maxStorageBufferRange) {
        vk_die("test requires ssbo size %u but the limit is %u", (unsigned)buf_size,
               limits->maxStorageBufferRange);
    }

    test->src_ubo_offset = ALIGN(alloc_size, limits->minUniformBufferOffsetAlignment);
    test->src_ubo_size = buf_size;
    alloc_size = test->src_ubo_offset + test->src_ubo_size;

    test->src_ssbo_offset = ALIGN(alloc_size, limits->minStorageBufferOffsetAlignment);
    test->src_ssbo_size = buf_size;
    alloc_size = test->src_ssbo_offset + test->src_ssbo_size;

    test->src_tbo_offset = ALIGN(alloc_size, limits->minTexelBufferOffsetAlignment);
    test->src_tbo_size = buf_size;
    alloc_size = test->src_tbo_offset + test->src_tbo_size;

    test->src_ibo_offset = ALIGN(alloc_size, limits->minTexelBufferOffsetAlignment);
    test->src_ibo_size = buf_size;
    alloc_size = test->src_ibo_offset + test->src_ibo_size;

    test->dst_ssbo_offset = ALIGN(alloc_size, limits->minStorageBufferOffsetAlignment);
    test->dst_ssbo_size = buf_size;
    alloc_size = test->dst_ssbo_offset + test->dst_ssbo_size;

    test->dst_ibo_offset = ALIGN(alloc_size, limits->minTexelBufferOffsetAlignment);
    test->dst_ibo_size = buf_size;
    alloc_size = test->dst_ibo_offset + test->dst_ibo_size;

    test->buf = vk_create_buffer(
        vk, 0, alloc_size,
        VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    VkBufferViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
        .buffer = test->buf->buf,
        .format = test->item_format,
    };

    view_info.offset = test->src_tbo_offset;
    view_info.range = test->src_tbo_size;
    vk->result = vk->CreateBufferView(vk->dev, &view_info, NULL, &test->src_tbo_view);
    vk_check(vk, "failed to create src tbo view");

    view_info.offset = test->src_ibo_offset;
    view_info.range = test->src_ibo_size;
    vk->result = vk->CreateBufferView(vk->dev, &view_info, NULL, &test->src_ibo_view);
    vk_check(vk, "failed to create src ibo view");

    view_info.offset = test->dst_ibo_offset;
    view_info.range = test->dst_ibo_size;
    vk->result = vk->CreateBufferView(vk->dev, &view_info, NULL, &test->dst_ibo_view);
    vk_check(vk, "failed to create dst ibo view");
}

static void
desc_buf_test_init(struct desc_buf_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);

    desc_buf_test_init_buffer(test);
    desc_buf_test_init_pipeline(test);
    desc_buf_test_init_descriptor_set(test);

    test->stopwatch = vk_create_stopwatch(vk, 2);
}

static void
desc_buf_test_cleanup(struct desc_buf_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_stopwatch(vk, test->stopwatch);

    vk_destroy_descriptor_set(vk, test->set);
    vk_destroy_pipeline(vk, test->pipeline);

    vk->DestroyBufferView(vk->dev, test->src_tbo_view, NULL);
    vk->DestroyBufferView(vk->dev, test->src_ibo_view, NULL);
    vk->DestroyBufferView(vk->dev, test->dst_ibo_view, NULL);
    vk_destroy_buffer(vk, test->buf);

    vk_cleanup(vk);
}

static void
desc_buf_test_dispatch(struct desc_buf_test *test, bool warmup)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, test->pipeline->pipeline);
    vk->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                              test->pipeline->pipeline_layout, 0, 1, &test->set->set, 0, NULL);

    if (test->global_size % test->local_size)
        vk_die("bad global/local sizes");
    const uint32_t group_count = test->global_size / test->local_size;

    if (warmup) {
        vk->CmdDispatch(cmd, group_count, 1, 1);
    } else {
        vk_write_stopwatch(vk, test->stopwatch, cmd);
        for (uint32_t i = 0; i < test->loop; i++)
            vk->CmdDispatch(cmd, group_count, 1, 1);
        vk_write_stopwatch(vk, test->stopwatch, cmd);
    }

    vk_end_cmd(vk);

    const uint64_t wait_begin = u_now();
    vk_wait(vk);
    const uint64_t wait_end = u_now();

    if (!warmup) {
        const float ns_per_ms = 1000000.0f;
        const float gpu_ms = (float)vk_read_stopwatch(vk, test->stopwatch, 0) / ns_per_ms;
        const float cpu_ms = (float)(wait_end - wait_begin) / ns_per_ms;
        vk_log("%dM threads, gpu time %.1fms, cpu wait time %.1fms",
               (test->global_size * test->loop) / 1000 / 1000, gpu_ms, cpu_ms);
    }
}

int
main(void)
{
    struct desc_buf_test test = {
        .global_size = 64 * 1024,
        .local_size = 64,
        .item_format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .item_size = 4 * 4,
        .loop = 10000,
    };

    desc_buf_test_init(&test);
    desc_buf_test_dispatch(&test, true);
    desc_buf_test_dispatch(&test, false);
    desc_buf_test_cleanup(&test);

    return 0;
}
