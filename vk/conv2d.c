/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static const uint32_t conv2d_test_cs[] = {
#include "conv2d_test.comp.inc"
};

struct conv2d_test_push_consts {
    uint32_t width;
    uint32_t slice;

    uint32_t kernel_width;
    uint32_t kernel_height;
};

struct conv2d_test {
    uint32_t width;
    uint32_t height;
    uint32_t slice;

    VkFormat type_format;
    uint32_t type_size;
    uint32_t type_width;

    uint32_t kernel_width;
    uint32_t kernel_height;

    uint32_t local_size;

    struct vk vk;

    struct vk_buffer *src;
    struct vk_buffer *dst;
    struct vk_buffer *weight;
    VkBufferView src_view;

    struct vk_pipeline *pipeline;
    struct vk_descriptor_set *set;
};

static void
conv2d_test_init_descriptor_set(struct conv2d_test *test)
{
    struct vk *vk = &test->vk;

    test->set = vk_create_descriptor_set(vk, test->pipeline->set_layouts[0]);

    const VkWriteDescriptorSet write_infos[] = {
        [0] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = test->set->set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
            .pTexelBufferView = &test->src_view,
        },
        [1] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = test->set->set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &(VkDescriptorBufferInfo){
                .buffer = test->dst->buf,
                .range = VK_WHOLE_SIZE,
            },
        },
        [2] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = test->set->set,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &(VkDescriptorBufferInfo){
                .buffer = test->weight->buf,
                .range = VK_WHOLE_SIZE,
            },
        },
    };
    vk->UpdateDescriptorSets(vk->dev, ARRAY_SIZE(write_infos), write_infos, 0, NULL);
}

static void
conv2d_test_init_pipeline(struct conv2d_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_COMPUTE_BIT, conv2d_test_cs,
                           sizeof(conv2d_test_cs));

    const VkDescriptorSetLayoutBinding bindings[] = {
        [0] = {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
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
                               sizeof(struct conv2d_test_push_consts));

    vk_setup_pipeline(vk, test->pipeline, NULL);
    vk_compile_pipeline(vk, test->pipeline);
}

static void
conv2d_test_init_buffers(struct conv2d_test *test)
{
    struct vk *vk = &test->vk;

    const VkDeviceSize vec_size = test->type_size * test->type_width;
    const VkDeviceSize mat_size = vec_size * test->type_width;

    const uint32_t src_pixel_count = (test->width + test->kernel_width - 1) *
                                     (test->height + test->kernel_height - 1) * test->slice;
    const uint32_t dst_pixel_count = test->width * test->height;
    const uint32_t weight_mat_count = test->kernel_width * test->kernel_height * test->slice;

    const VkDeviceSize src_buf_size = src_pixel_count * vec_size;
    const VkDeviceSize dst_buf_size = dst_pixel_count * vec_size;
    const VkDeviceSize weight_buf_size = weight_mat_count * mat_size;

    test->src = vk_create_buffer(vk, 0, src_buf_size, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
    test->dst = vk_create_buffer(vk, 0, dst_buf_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    test->weight = vk_create_buffer(vk, 0, weight_buf_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    const VkBufferViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
        .buffer = test->src->buf,
        .format = test->type_format,
        .range = VK_WHOLE_SIZE,
    };
    vk->result = vk->CreateBufferView(vk->dev, &view_info, NULL, &test->src_view);
    vk_check(vk, "failed to create src view");
}

static void
conv2d_test_init(struct conv2d_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);

    conv2d_test_init_buffers(test);
    conv2d_test_init_pipeline(test);
    conv2d_test_init_descriptor_set(test);
}

static void
conv2d_test_cleanup(struct conv2d_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_descriptor_set(vk, test->set);
    vk_destroy_pipeline(vk, test->pipeline);
    vk->DestroyBufferView(vk->dev, test->src_view, NULL);
    vk_destroy_buffer(vk, test->weight);
    vk_destroy_buffer(vk, test->dst);
    vk_destroy_buffer(vk, test->src);

    vk_cleanup(vk);
}

static void
conv2d_test_dispatch(struct conv2d_test *test, bool warmup)
{
    struct vk *vk = &test->vk;
    struct vk_stopwatch *stopwatch = warmup ? NULL : vk_create_stopwatch(vk, 2);

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, test->pipeline->pipeline);
    vk->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                              test->pipeline->pipeline_layout, 0, 1, &test->set->set, 0, NULL);

    const struct conv2d_test_push_consts consts = {
        .width = test->width,
        .slice = test->slice,
        .kernel_width = test->kernel_width,
        .kernel_height = test->kernel_height,
    };
    vk->CmdPushConstants(cmd, test->pipeline->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(consts), &consts);

    if (stopwatch)
        vk_write_stopwatch(vk, stopwatch, cmd);
    vk->CmdDispatch(cmd, test->width / test->local_size, test->height, 1);
    if (stopwatch)
        vk_write_stopwatch(vk, stopwatch, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    if (stopwatch) {
        const float ns_per_ms = 1000000.0f;
        const float gpu_ms = (float)vk_read_stopwatch(vk, stopwatch, 0) / ns_per_ms;
        vk_log("gpu %.1fms", gpu_ms);
        vk_destroy_stopwatch(vk, stopwatch);
    }
}

int
main(void)
{
    struct conv2d_test test = {
        .width = 512,
        .height = 288,
        .slice = 6,

        .type_format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .type_size = 4,
        .type_width = 4,

        .kernel_width = 3,
        .kernel_height = 3,

        .local_size = 64,
    };

    if (test.width % test.local_size)
        vk_die("bad width / local size");

    conv2d_test_init(&test);
    conv2d_test_dispatch(&test, true);
    conv2d_test_dispatch(&test, false);
    conv2d_test_cleanup(&test);

    return 0;
}
