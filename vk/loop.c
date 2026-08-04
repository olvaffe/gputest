/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static const uint32_t loop_test_cs[] = {
#include "loop_test.comp.inc"
};

struct loop_test_push_consts {
    uint32_t repeat;
};

struct loop_test {
    uint32_t buf_width;
    uint32_t type_size;
    uint32_t local_size;

    struct vk vk;

    struct vk_pipeline *pipeline;

    struct vk_buffer *src;
    struct vk_buffer *dst;
    struct vk_buffer *weight;
    struct vk_descriptor_set *set;
};

static void
loop_test_init_descriptor_set(struct loop_test *test)
{
    struct vk *vk = &test->vk;

    test->set = vk_create_descriptor_set(vk, test->pipeline->set_layouts[0]);

    vk_write_descriptor_set_buffer(vk, test->set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, test->dst,
                                   VK_WHOLE_SIZE);
}

static void
loop_test_init_pipeline(struct loop_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_COMPUTE_BIT, loop_test_cs,
                           sizeof(loop_test_cs));

    vk_add_pipeline_set_layout(vk, test->pipeline, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                               VK_SHADER_STAGE_COMPUTE_BIT, NULL);

    test->pipeline->push_const = (VkPushConstantRange){
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .size = sizeof(struct loop_test_push_consts),
    };

    vk_compile_pipeline(vk, test->pipeline);
}

static void
loop_test_init_buffer(struct loop_test *test)
{
    struct vk *vk = &test->vk;

    const VkDeviceSize buf_size = test->buf_width * test->type_size;
    test->dst = vk_create_buffer(vk, 0, buf_size, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT);
}

static void
loop_test_init(struct loop_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);

    loop_test_init_pipeline(test);
    loop_test_init_buffer(test);
    loop_test_init_descriptor_set(test);
}

static void
loop_test_cleanup(struct loop_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_descriptor_set(vk, test->set);
    vk_destroy_buffer(vk, test->dst);
    vk_destroy_pipeline(vk, test->pipeline);

    vk_cleanup(vk);
}

static void
loop_test_dispatch(struct loop_test *test)
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

    const struct loop_test_push_consts consts = {
        .repeat = 100,
    };
    const VkPushConstantsInfo push_info = {
        .sType = VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO,
        .layout = test->pipeline->layout,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .size = sizeof(consts),
        .pValues = &consts,
    };
    vk->CmdPushConstants2(cmd, &push_info);

    vk->CmdDispatch(cmd, test->buf_width / test->local_size, 1, 1);

    vk_end_cmd(vk);
    vk_wait(vk);
}

int
main(void)
{
    struct loop_test test = {
        .buf_width = 64 * 64,
        .type_size = 2 * 1,
        .local_size = 64,
    };

    loop_test_init(&test);
    loop_test_dispatch(&test);
    loop_test_cleanup(&test);

    return 0;
}
