/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static const uint32_t conv1d_test_cs[] = {
#include "conv1d_test.comp.inc"
};

struct conv1d_test {
    uint32_t width;
    uint32_t type_size;
    uint32_t type_width;
    uint32_t local_size;
    struct conv1d_test_params {
        uint32_t kernel_size;
        uint32_t repeat;
    } params;

    struct vk vk;

    struct vk_buffer *src;
    struct vk_buffer *dst;
    VkDeviceSize buf_size;

    struct vk_pipeline *pipeline;
    struct vk_descriptor_set *set;
};

static void
conv1d_test_init_descriptor_set(struct conv1d_test *test)
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
                .buffer = test->src->buf,
                .range = test->buf_size,
            },
        },
        [1] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = test->set->set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &(VkDescriptorBufferInfo){
                .buffer = test->dst->buf,
                .range = test->buf_size,
            },
        },
    };
    vk->UpdateDescriptorSets(vk->dev, ARRAY_SIZE(write_infos), write_infos, 0, NULL);
}

static void
conv1d_test_init_pipeline(struct conv1d_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_COMPUTE_BIT, conv1d_test_cs,
                           sizeof(conv1d_test_cs));

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

    vk_set_pipeline_push_const(vk, test->pipeline, VK_SHADER_STAGE_COMPUTE_BIT,
                               sizeof(test->params));

    vk_setup_pipeline(vk, test->pipeline, NULL);
    vk_compile_pipeline(vk, test->pipeline);
}

static void
conv1d_test_init_buffers(struct conv1d_test *test)
{
    struct vk *vk = &test->vk;

    const VkDeviceSize buf_size = test->width * test->type_size * test->type_width;
    test->src = vk_create_buffer(vk, 0, buf_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    test->dst = vk_create_buffer(vk, 0, buf_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    test->buf_size = buf_size;
}

static void
conv1d_test_init(struct conv1d_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);

    conv1d_test_init_buffers(test);
    conv1d_test_init_pipeline(test);
    conv1d_test_init_descriptor_set(test);
}

static void
conv1d_test_cleanup(struct conv1d_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_descriptor_set(vk, test->set);
    vk_destroy_pipeline(vk, test->pipeline);
    vk_destroy_buffer(vk, test->dst);
    vk_destroy_buffer(vk, test->src);

    vk_cleanup(vk);
}

static void
conv1d_test_dispatch(struct conv1d_test *test, bool warmup)
{
    struct vk *vk = &test->vk;
    struct vk_stopwatch *stopwatch = warmup ? NULL : vk_create_stopwatch(vk, 2);

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, test->pipeline->pipeline);
    vk->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                              test->pipeline->pipeline_layout, 0, 1, &test->set->set, 0, NULL);

    struct conv1d_test_params params = test->params;
    if (warmup)
        params.repeat = 1;

    vk->CmdPushConstants(cmd, test->pipeline->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(params), &params);

    if (stopwatch)
        vk_write_stopwatch(vk, stopwatch, cmd);
    vk->CmdDispatch(cmd, test->width / test->local_size, 1, 1);
    if (stopwatch)
        vk_write_stopwatch(vk, stopwatch, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    if (stopwatch) {
        const float ns_per_ms = 1000000.0f;
        const float gpu_ms = (float)vk_read_stopwatch(vk, stopwatch, 0) / ns_per_ms;
        vk_log("buf width %d, type size %dx%d, kernel size %d, repeat %d: %.1fms", test->width,
               test->type_size, test->type_width, params.kernel_size, params.repeat, gpu_ms);

        vk_destroy_stopwatch(vk, stopwatch);
    }
}

int
main(void)
{
    struct conv1d_test test = {
        .width = 64 * 64,
        .type_size = 2,
        .type_width = 2,
        .local_size = 8,
        .params = {
            .kernel_size = 8,
            .repeat = 10000,
        },
    };

    conv1d_test_init(&test);
    conv1d_test_dispatch(&test, true);
    conv1d_test_dispatch(&test, false);
    conv1d_test_cleanup(&test);

    return 0;
}
