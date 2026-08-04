/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static const uint32_t subgroup_test_cs[] = {
#include "subgroup_test.comp.inc"
};

struct subgroup_test {
    float vals[2];

    struct vk vk;

    struct vk_pipeline *pipeline;

    struct vk_buffer *src;
    struct vk_buffer *dst;
    struct vk_descriptor_set *set;
};

static void
subgroup_test_init_descriptor_set(struct subgroup_test *test)
{
    struct vk *vk = &test->vk;

    test->set = vk_create_descriptor_set(vk, test->pipeline->set_layouts[0]);

    const VkWriteDescriptorSet write_infos[] = {
        [0] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = test->set->set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &(VkDescriptorBufferInfo){
                .buffer = test->dst->buf,
                .range = VK_WHOLE_SIZE,
            },
        },
        [1] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = test->set->set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &(VkDescriptorBufferInfo){
                .buffer = test->src->buf,
                .range = VK_WHOLE_SIZE,
            },
        },
    };
    vk->UpdateDescriptorSets(vk->dev, ARRAY_SIZE(write_infos), write_infos, 0, NULL);
}

static void
subgroup_test_init_pipeline(struct subgroup_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_COMPUTE_BIT, subgroup_test_cs,
                           sizeof(subgroup_test_cs));

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
    vk_add_pipeline_set_layout_from_info(vk, test->pipeline, &set_layout_info);

    vk_compile_pipeline(vk, test->pipeline);
}

static void
subgroup_test_init_buffers(struct subgroup_test *test)
{
    struct vk *vk = &test->vk;

    const VkDeviceSize size = sizeof(test->vals);
    test->src = vk_create_buffer(vk, 0, size, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT);
    test->dst = vk_create_buffer(vk, 0, size, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT);

    memcpy(test->src->mem_ptr, test->vals, size);
}

static void
subgroup_test_init(struct subgroup_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);

    subgroup_test_init_pipeline(test);
    subgroup_test_init_buffers(test);
    subgroup_test_init_descriptor_set(test);
}

static void
subgroup_test_cleanup(struct subgroup_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_descriptor_set(vk, test->set);
    vk_destroy_buffer(vk, test->dst);
    vk_destroy_buffer(vk, test->src);
    vk_destroy_pipeline(vk, test->pipeline);

    vk_cleanup(vk);
}

static void
subgroup_test_dispatch(struct subgroup_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    vk_bind_pipeline(vk, test->pipeline, cmd);
    const VkBindDescriptorSetsInfo bind_info = {
        .sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .layout = test->pipeline->layout,
        .descriptorSetCount = 1,
        .pDescriptorSets = &test->set->set,
    };
    vk->CmdBindDescriptorSets2(cmd, &bind_info);

    const VkBufferMemoryBarrier2 before_barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .buffer = test->src->buf,
        .size = VK_WHOLE_SIZE,
    };
    const VkDependencyInfo dep_info1 = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &before_barrier,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info1);

    vk->CmdDispatch(cmd, 1, 1, 1);

    const VkBufferMemoryBarrier2 after_barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
        .buffer = test->dst->buf,
        .size = VK_WHOLE_SIZE,
    };
    const VkDependencyInfo dep_info2 = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &after_barrier,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info2);

    vk_end_cmd(vk);
    vk_wait(vk);

    float sum = 0.0f;
    for (uint32_t i = 0; i < ARRAY_SIZE(test->vals); i++)
        sum += test->vals[i];

    const float *res = (const float *)test->dst->mem_ptr;
    for (uint32_t i = 0; i < ARRAY_SIZE(test->vals); i++) {
        if (res[i] != sum)
            vk_log("bad res[%d] is %f, not %f", i, res[i], sum);
    }
}

int
main(void)
{
    struct subgroup_test test = {
        .vals = { 0.5f, 0.25f },
    };

    subgroup_test_init(&test);
    subgroup_test_dispatch(&test);
    subgroup_test_cleanup(&test);

    return 0;
}
