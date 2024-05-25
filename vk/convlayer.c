/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static const uint32_t convlayer_test_cs[] = {
#include "convlayer_test.comp.inc"
};

struct convlayer_test_ubo {
    uint32_t src_slice_count;
    uint32_t dst_slice_count;
    uint32_t grid_width;
    uint32_t grid_height;
};

struct convlayer_test {
    VkFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t src_slice_count;
    uint32_t dst_slice_count;

    uint32_t grid_width;
    uint32_t grid_height;

    uint32_t local_size[3];
    uint32_t block_size[3];

    struct vk vk;

    struct vk_buffer *ssbo;
    struct vk_buffer *ubo;
    struct vk_image *src;
    struct vk_image *dst;

    struct vk_pipeline *pipeline;
    struct vk_descriptor_set *set;
};

static void
convlayer_test_init_descriptor_set(struct convlayer_test *test)
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
                .buffer = test->ssbo->buf,
                .range = VK_WHOLE_SIZE,
            },
        },
        [1] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = test->set->set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &(VkDescriptorBufferInfo){
                .buffer = test->ubo->buf,
                .range = VK_WHOLE_SIZE,
            },
        },
        [2] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = test->set->set,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = &(VkDescriptorImageInfo){
                .imageView = test->src->sample_view,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            },
        },
        [3] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = test->set->set,
            .dstBinding = 3,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &(VkDescriptorImageInfo){
                .imageView = test->dst->render_view,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            },
        },
    };
    vk->UpdateDescriptorSets(vk->dev, ARRAY_SIZE(write_infos), write_infos, 0, NULL);
}

static void
convlayer_test_init_pipeline(struct convlayer_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_COMPUTE_BIT, convlayer_test_cs,
                           sizeof(convlayer_test_cs));

    const VkDescriptorSetLayoutBinding bindings[] = {
        [0] = {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        [1] = {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        [2] = {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        [3] = {
            .binding = 3,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
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

    /* unused */
    vk_set_pipeline_push_const(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, 8);

    vk_setup_pipeline(vk, test->pipeline, NULL);
    vk_compile_pipeline(vk, test->pipeline);
}

static void
convlayer_test_init_images(struct convlayer_test *test)
{
    struct vk *vk = &test->vk;

    test->src =
        vk_create_image(vk, test->format, test->width, test->height * test->src_slice_count,
                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    vk_create_image_sample_view(vk, test->src, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);

    test->dst =
        vk_create_image(vk, test->format, test->width, test->height * test->dst_slice_count,
                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    vk_create_image_render_view(vk, test->dst, VK_IMAGE_ASPECT_COLOR_BIT);
}

static void
convlayer_test_init_buffers(struct convlayer_test *test)
{
    struct vk *vk = &test->vk;

    const VkDeviceSize weight_size = 4 * 4 * 2; /* f16mat4 */
    const VkDeviceSize weight_count = test->src_slice_count * test->dst_slice_count;
    const VkDeviceSize ssbo_size = weight_size * weight_count;
    test->ssbo =
        vk_create_buffer(vk, 0, ssbo_size,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    test->ubo =
        vk_create_buffer(vk, 0, sizeof(struct convlayer_test_ubo),
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    struct convlayer_test_ubo *ubo = test->ubo->mem_ptr;
    ubo->src_slice_count = test->src_slice_count;
    ubo->dst_slice_count = test->dst_slice_count;
    ubo->grid_width = test->grid_width;
    ubo->grid_height = test->grid_height;
}

static void
convlayer_test_init(struct convlayer_test *test)
{
    struct vk *vk = &test->vk;

    const struct vk_init_params params = {
        .api_version = VK_API_VERSION_1_2,
        .enable_all_features = true,
    };
    vk_init(vk, &params);

    convlayer_test_init_buffers(test);
    convlayer_test_init_images(test);
    convlayer_test_init_pipeline(test);
    convlayer_test_init_descriptor_set(test);
}

static void
convlayer_test_cleanup(struct convlayer_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_descriptor_set(vk, test->set);
    vk_destroy_pipeline(vk, test->pipeline);
    vk_destroy_image(vk, test->src);
    vk_destroy_image(vk, test->dst);
    vk_destroy_buffer(vk, test->ssbo);
    vk_destroy_buffer(vk, test->ubo);

    vk_cleanup(vk);
}

static void
convlayer_test_dispatch(struct convlayer_test *test, bool warmup)
{
    struct vk *vk = &test->vk;
    struct vk_stopwatch *stopwatch = warmup ? NULL : vk_create_stopwatch(vk, 2);

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, test->pipeline->pipeline);

    const VkImageMemoryBarrier barriers[] = {
        [0] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .image = test->dst->img,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        },
        [1] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .image = test->src->img,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        }
    };
    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL,
                           ARRAY_SIZE(barriers), barriers);

    vk->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                              test->pipeline->pipeline_layout, 0, 1, &test->set->set, 0, NULL);

    const uint32_t dispatch_width =
        DIV_ROUND_UP(test->grid_width, test->local_size[0] * test->block_size[0]);
    const uint32_t dispatch_height =
        DIV_ROUND_UP(test->grid_height, test->local_size[1] * test->block_size[1]);
    const uint32_t dispatch_depth =
        DIV_ROUND_UP(test->dst_slice_count, test->local_size[2] * test->block_size[2]);

    if (stopwatch)
        vk_write_stopwatch(vk, stopwatch, cmd);
    vk->CmdDispatch(cmd, dispatch_width, dispatch_height, dispatch_depth);
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
    struct convlayer_test test = {
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .width = 1024,
        .height = 1,
        .src_slice_count = 3072,
        .dst_slice_count = 384,

        .grid_width = 256,
        .grid_height = 1,

        .local_size = { 16, 1, 16 },
        .block_size = { 4, 1, 4 },
    };

    convlayer_test_init(&test);
    convlayer_test_dispatch(&test, true);
    convlayer_test_dispatch(&test, false);
    convlayer_test_cleanup(&test);

    return 0;
}
