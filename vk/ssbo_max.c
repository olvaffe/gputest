/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

/* This allocates an SSBO that is close to maxStorageBufferRange and verifies
 * that it can be written to.
 */

#include "vkutil.h"

static const uint32_t ssbo_max_test_cs[] = {
#include "ssbo_max_test.comp.inc"
};

struct ssbo_max_test {
    uint32_t local_size;

    struct vk vk;
    uint32_t grid_size;
    struct vk_buffer *ssbo;

    struct vk_pipeline *pipeline;
    struct vk_descriptor_set *set;
};

static void
ssbo_max_test_init_descriptor_set(struct ssbo_max_test *test)
{
    struct vk *vk = &test->vk;

    test->set = vk_create_descriptor_set(vk, test->pipeline->set_layouts[0]);
    vk_write_descriptor_set_buffer(vk, test->set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, test->ssbo,
                                   VK_WHOLE_SIZE);
}

static void
ssbo_max_test_init_pipeline(struct ssbo_max_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_COMPUTE_BIT, ssbo_max_test_cs,
                           sizeof(ssbo_max_test_cs));

    vk_add_pipeline_set_layout(vk, test->pipeline, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                               VK_SHADER_STAGE_COMPUTE_BIT, NULL);

    vk_setup_pipeline(vk, test->pipeline, NULL);
    vk_compile_pipeline(vk, test->pipeline);
}

static void
ssbo_max_test_init_ssbo(struct ssbo_max_test *test)
{
    struct vk *vk = &test->vk;
    const VkPhysicalDeviceLimits *limits = &vk->props.properties.limits;

    test->grid_size = (uint32_t)sqrt((double)(limits->maxStorageBufferRange / sizeof(uint32_t)));

    VkDeviceSize size = test->grid_size * test->grid_size * sizeof(uint32_t);
    test->ssbo = vk_create_buffer(vk, 0, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    memset(test->ssbo->mem_ptr, 0, size);
}

static void
ssbo_max_test_init(struct ssbo_max_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);
    ssbo_max_test_init_ssbo(test);

    ssbo_max_test_init_pipeline(test);
    ssbo_max_test_init_descriptor_set(test);
}

static void
ssbo_max_test_cleanup(struct ssbo_max_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_descriptor_set(vk, test->set);
    vk_destroy_pipeline(vk, test->pipeline);

    vk_destroy_buffer(vk, test->ssbo);

    vk_cleanup(vk);
}

static void
ssbo_max_test_dispatch_ssbo(struct ssbo_max_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkBufferMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .buffer = test->ssbo->buf,
        .size = VK_WHOLE_SIZE,
    };

    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, test->pipeline->pipeline);

    vk->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                              test->pipeline->pipeline_layout, 0, 1, &test->set->set, 0, NULL);

    const uint32_t count = test->grid_size / test->local_size;
    vk->CmdDispatch(cmd, count, count, 1);

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                           0, 0, NULL, 1, &barrier, 0, NULL);
}

static void
ssbo_max_test_dispatch(struct ssbo_max_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    ssbo_max_test_dispatch_ssbo(test, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    vk_log("checking %ux%u", test->grid_size, test->grid_size);
    for (uint32_t y = 0; y < test->grid_size; y++) {
        for (uint32_t x = 0; x < test->grid_size; x++) {
            const uint32_t *data = test->ssbo->mem_ptr;
            const uint32_t off = test->grid_size * y + x;
            const uint32_t val = data[off];

            if (off != val)
                vk_die("data[%u] is %u, not %u\n", off, val, off);
        }
    }
}

int
main(void)
{
    struct ssbo_max_test test = {
        .local_size = 8,
    };

    ssbo_max_test_init(&test);
    ssbo_max_test_dispatch(&test);
    ssbo_max_test_cleanup(&test);

    return 0;
}
