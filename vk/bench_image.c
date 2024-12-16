/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static const uint32_t bench_image_test_vs[] = {
#include "bench_image_test.vert.inc"
};

static const uint32_t bench_image_test_fs[] = {
#include "bench_image_test.frag.inc"
};

static const uint32_t bench_image_test_cs[] = {
#include "bench_image_test.comp.inc"
};

struct bench_image_test {
    VkFormat format;
    uint32_t elem_size;
    uint32_t width;
    uint32_t height;
    uint32_t loop;

    uint32_t cs_local_size;

    struct vk vk;
    struct vk_stopwatch *stopwatch;
};

static void
bench_image_test_init(struct bench_image_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);

    test->stopwatch = vk_create_stopwatch(vk, 2);
}

static void
bench_image_test_cleanup(struct bench_image_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_stopwatch(vk, test->stopwatch);
    vk_cleanup(vk);
}

static const char *
bench_image_test_describe_mt(struct bench_image_test *test,
                             VkImageTiling tiling,
                             uint32_t mt_idx,
                             char desc[static 64])
{
    struct vk *vk = &test->vk;
    const VkMemoryType *mt = &vk->mem_props.memoryTypes[mt_idx];
    const bool mt_local = mt->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    const bool mt_coherent = mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    const bool mt_cached = mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

    snprintf(desc, 64, "%s mt %d (%s%s%s)",
             tiling == VK_IMAGE_TILING_LINEAR ? "linear" : "optimal", mt_idx,
             mt_local ? "Lo" : "..", mt_coherent ? "Co" : "..", mt_cached ? "Ca" : "..");

    return desc;
}

static uint64_t
bench_image_test_calc_throughput(struct bench_image_test *test, uint64_t dur)
{
    const uint64_t ns_per_s = 1000000000;

    return test->elem_size * test->width * test->height * test->loop * ns_per_s / dur;
}

static uint32_t
bench_image_test_calc_throughput_mb(struct bench_image_test *test, uint64_t dur)
{
    return bench_image_test_calc_throughput(test, dur) / 1024 / 1024;
}

static uint64_t
bench_image_test_clear(struct bench_image_test *test, struct vk_image *img)
{
    struct vk *vk = &test->vk;

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = img->img,
        .subresourceRange = subres_range,
    };
    const VkClearColorValue clear_val = {
        .float32 = { 0.5f, 0.5f, 0.5f, 0.5f },
    };

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);
    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0, 0, NULL, 0, NULL, 1, &barrier);
    vk->CmdClearColorImage(cmd, img->img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_val, 1,
                           &subres_range);
    vk_end_cmd(vk);
    vk_wait(vk);

    cmd = vk_begin_cmd(vk, false);
    vk_write_stopwatch(vk, test->stopwatch, cmd);
    for (uint32_t i = 0; i < test->loop; i++) {
        vk->CmdClearColorImage(cmd, img->img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_val, 1,
                               &subres_range);
    }
    vk_write_stopwatch(vk, test->stopwatch, cmd);
    vk_end_cmd(vk);
    vk_wait(vk);

    const uint64_t dur = vk_read_stopwatch(vk, test->stopwatch, 0);
    vk_reset_stopwatch(vk, test->stopwatch);

    return dur;
}

static uint64_t
bench_image_test_copy(struct bench_image_test *test, struct vk_image *dst, struct vk_image *src)
{
    struct vk *vk = &test->vk;

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier barriers[2] = {
        [0] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = dst->img,
            .subresourceRange = subres_range,
        },
        [1] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .image = src->img,
            .subresourceRange = subres_range,
        },
    };

    const VkImageSubresourceLayers subres_layers = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .layerCount = 1,
    };
    const VkImageCopy copy = { .srcSubresource = subres_layers,
                               .dstSubresource = subres_layers,
                               .extent = {
                                   .width = test->width,
                                   .height = test->height,
                                   .depth = 1,
                               } };

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);
    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0, 0, NULL, 0, NULL, ARRAY_SIZE(barriers), barriers);
    vk->CmdCopyImage(cmd, src->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->img,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    vk_end_cmd(vk);
    vk_wait(vk);

    cmd = vk_begin_cmd(vk, false);
    vk_write_stopwatch(vk, test->stopwatch, cmd);
    for (uint32_t i = 0; i < test->loop; i++) {
        vk->CmdCopyImage(cmd, src->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->img,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    }
    vk_write_stopwatch(vk, test->stopwatch, cmd);
    vk_end_cmd(vk);
    vk_wait(vk);

    const uint64_t dur = vk_read_stopwatch(vk, test->stopwatch, 0);
    vk_reset_stopwatch(vk, test->stopwatch);

    return dur;
}

static uint64_t
bench_image_test_dispatch(struct bench_image_test *test,
                          struct vk_image *dst,
                          struct vk_image *src)
{
    struct vk *vk = &test->vk;
    struct vk_pipeline *pipeline;
    struct vk_descriptor_set *set;

    {
        pipeline = vk_create_pipeline(vk);

        vk_add_pipeline_shader(vk, pipeline, VK_SHADER_STAGE_COMPUTE_BIT, bench_image_test_cs,
                               sizeof(bench_image_test_cs));

        const VkDescriptorSetLayoutBinding bindings[] = {
            [0] = {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            },
            [1] = {
                .binding = 1,
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
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = &(VkDescriptorImageInfo){
                    .imageView = dst->render_view,
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                },
            },
            [1] = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = set->set,
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = &(VkDescriptorImageInfo){
                    .imageView = src->render_view,
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                },
            },
        };
        vk->UpdateDescriptorSets(vk->dev, ARRAY_SIZE(write_infos), write_infos, 0, NULL);
    }

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier barriers[2] = {
        [0] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .image = dst->img,
            .subresourceRange = subres_range,
        },
        [1] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .image = src->img,
            .subresourceRange = subres_range,
        },
    };

    assert((test->width | test->height) % test->cs_local_size == 0);
    const uint32_t group_count_x = test->width / test->cs_local_size;
    const uint32_t group_count_y = test->height / test->cs_local_size;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);
    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL,
                           ARRAY_SIZE(barriers), barriers);
    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);
    vk->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline_layout, 0,
                              1, &set->set, 0, NULL);
    vk->CmdDispatch(cmd, group_count_x, group_count_y, 1);
    vk_end_cmd(vk);
    vk_wait(vk);

    cmd = vk_begin_cmd(vk, false);
    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);
    vk->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline_layout, 0,
                              1, &set->set, 0, NULL);
    vk_write_stopwatch(vk, test->stopwatch, cmd);
    for (uint32_t i = 0; i < test->loop; i++)
        vk->CmdDispatch(cmd, group_count_x, group_count_y, 1);
    vk_write_stopwatch(vk, test->stopwatch, cmd);
    vk_end_cmd(vk);
    vk_wait(vk);

    vk_destroy_pipeline(vk, pipeline);
    vk_destroy_descriptor_set(vk, set);

    const uint64_t dur = vk_read_stopwatch(vk, test->stopwatch, 0);
    vk_reset_stopwatch(vk, test->stopwatch);

    return dur;
}

static uint64_t
bench_image_test_render_pass(struct bench_image_test *test,
                             struct vk_image *dst,
                             struct vk_image *src)
{
    struct vk *vk = &test->vk;
    struct vk_framebuffer *fb;
    struct vk_pipeline *pipeline;
    struct vk_descriptor_set *set;

    {
        fb = vk_create_framebuffer(vk, dst, NULL, NULL, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                   VK_ATTACHMENT_STORE_OP_STORE);
    }

    {
        pipeline = vk_create_pipeline(vk);

        vk_add_pipeline_shader(vk, pipeline, VK_SHADER_STAGE_VERTEX_BIT, bench_image_test_vs,
                               sizeof(bench_image_test_vs));
        vk_add_pipeline_shader(vk, pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, bench_image_test_fs,
                               sizeof(bench_image_test_fs));

        vk_add_pipeline_set_layout(vk, pipeline, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1,
                                   VK_SHADER_STAGE_FRAGMENT_BIT, NULL);

        vk_set_pipeline_topology(vk, pipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
        vk_set_pipeline_viewport(vk, pipeline, test->width, test->height);
        vk_set_pipeline_rasterization(vk, pipeline, VK_POLYGON_MODE_FILL);
        vk_set_pipeline_sample_count(vk, pipeline, VK_SAMPLE_COUNT_1_BIT);

        vk_setup_pipeline(vk, pipeline, fb);
        vk_compile_pipeline(vk, pipeline);
    }

    {
        set = vk_create_descriptor_set(vk, pipeline->set_layouts[0]);
        const VkWriteDescriptorSet write_info = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = set->set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo =
                &(VkDescriptorImageInfo){
                    .imageView = src->sample_view,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                },
        };
        vk->UpdateDescriptorSets(vk->dev, 1, &write_info, 0, NULL);
    }

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier barriers[2] = {
        [0] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .image = dst->img,
            .subresourceRange = subres_range,
        },
        [1] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .image = src->img,
            .subresourceRange = subres_range,
        },
    };
    const VkRenderPassBeginInfo pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = fb->pass,
        .framebuffer = fb->fb,
        .renderArea = {
            .extent = {
                .width = test->width,
                .height = test->height,
            },
        },
    };

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);
    vk->CmdPipelineBarrier(
        cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
        0, NULL, 0, NULL, ARRAY_SIZE(barriers), barriers);
    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
    vk->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 0,
                              1, &set->set, 0, NULL);
    vk->CmdBeginRenderPass(cmd, &pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vk->CmdDraw(cmd, 4, 1, 0, 0);
    vk->CmdEndRenderPass(cmd);
    vk_end_cmd(vk);
    vk_wait(vk);

    cmd = vk_begin_cmd(vk, false);
    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
    vk->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 0,
                              1, &set->set, 0, NULL);
    vk_write_stopwatch(vk, test->stopwatch, cmd);
    vk->CmdBeginRenderPass(cmd, &pass_info, VK_SUBPASS_CONTENTS_INLINE);
    for (uint32_t i = 0; i < test->loop; i++)
        vk->CmdDraw(cmd, 4, 1, 0, 0);
    vk->CmdEndRenderPass(cmd);
    vk_write_stopwatch(vk, test->stopwatch, cmd);
    vk_end_cmd(vk);
    vk_wait(vk);

    vk_destroy_pipeline(vk, pipeline);
    vk_destroy_descriptor_set(vk, set);
    vk_destroy_framebuffer(vk, fb);

    const uint64_t dur = vk_read_stopwatch(vk, test->stopwatch, 0);
    vk_reset_stopwatch(vk, test->stopwatch);

    return dur;
}

static void
bench_image_test_init_info(struct bench_image_test *test,
                           VkImageTiling tiling,
                           VkImageUsageFlags usage,
                           VkImageCreateInfo *info)
{
    *info = (VkImageCreateInfo){
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = test->format,
        .extent = {
            .width = test->width,
            .height = test->height,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = tiling,
        .usage = usage,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
}

static void
bench_image_test_draw_clear(struct bench_image_test *test, VkImageTiling tiling)
{
    struct vk *vk = &test->vk;
    char desc[64];

    const VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkImageCreateInfo info;
    bench_image_test_init_info(test, tiling, usage, &info);

    const uint32_t mt_mask = vk_get_image_mt_mask(vk, &info);

    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        if (!(mt_mask & (1 << i)))
            continue;

        struct vk_image *img = vk_create_image_with_mt(vk, &info, i);

        const uint64_t dur = bench_image_test_clear(test, img);

        vk_destroy_image(vk, img);

        vk_log("%s: vkCmdClearColorImage: %d MB/s",
               bench_image_test_describe_mt(test, tiling, i, desc),
               bench_image_test_calc_throughput_mb(test, dur));
    }
}

static void
bench_image_test_draw_copy(struct bench_image_test *test, VkImageTiling tiling)
{
    struct vk *vk = &test->vk;
    char desc[64];

    const VkImageUsageFlags usage =
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkImageCreateInfo info;
    bench_image_test_init_info(test, tiling, usage, &info);

    const uint32_t mt_mask = vk_get_image_mt_mask(vk, &info);

    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        if (!(mt_mask & (1 << i)))
            continue;

        struct vk_image *dst = vk_create_image_with_mt(vk, &info, i);
        struct vk_image *src = vk_create_image_with_mt(vk, &info, i);

        const uint64_t dur = bench_image_test_copy(test, dst, src);

        vk_destroy_image(vk, dst);
        vk_destroy_image(vk, src);

        vk_log("%s: vkCmdCopyImage: %d MB/s", bench_image_test_describe_mt(test, tiling, i, desc),
               bench_image_test_calc_throughput_mb(test, dur));
    }
}

static void
bench_image_test_draw_compute(struct bench_image_test *test, VkImageTiling tiling)
{
    struct vk *vk = &test->vk;
    char desc[64];

    const VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT;
    VkImageCreateInfo info;
    bench_image_test_init_info(test, tiling, usage, &info);

    const uint32_t mt_mask = vk_get_image_mt_mask(vk, &info);

    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        if (!(mt_mask & (1 << i)))
            continue;

        struct vk_image *dst = vk_create_image_with_mt(vk, &info, i);
        struct vk_image *src = vk_create_image_with_mt(vk, &info, i);
        vk_create_image_render_view(vk, dst, VK_IMAGE_ASPECT_COLOR_BIT);
        vk_create_image_render_view(vk, src, VK_IMAGE_ASPECT_COLOR_BIT);

        const uint64_t dur = bench_image_test_dispatch(test, dst, src);

        vk_destroy_image(vk, dst);
        vk_destroy_image(vk, src);

        vk_log("%s: compute: %d MB/s", bench_image_test_describe_mt(test, tiling, i, desc),
               bench_image_test_calc_throughput_mb(test, dur));
    }
}

static void
bench_image_test_draw_quad(struct bench_image_test *test, VkImageTiling tiling)
{
    struct vk *vk = &test->vk;
    char desc[64];

    const VkImageUsageFlags dst_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    const VkImageUsageFlags src_usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImageCreateInfo dst_info;
    VkImageCreateInfo src_info;
    bench_image_test_init_info(test, tiling, dst_usage, &dst_info);
    bench_image_test_init_info(test, tiling, src_usage, &src_info);

    const uint32_t dst_mt_mask = vk_get_image_mt_mask(vk, &dst_info);
    const uint32_t src_mt_mask = vk_get_image_mt_mask(vk, &src_info);
    const uint32_t mt_mask = dst_mt_mask & src_mt_mask;

    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        if (!(mt_mask & (1 << i)))
            continue;

        struct vk_image *dst = vk_create_image_with_mt(vk, &dst_info, i);
        struct vk_image *src = vk_create_image_with_mt(vk, &src_info, i);

        vk_create_image_render_view(vk, dst, VK_IMAGE_ASPECT_COLOR_BIT);
        vk_create_image_sample_view(vk, src, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);

        const uint64_t dur = bench_image_test_render_pass(test, dst, src);

        vk_destroy_image(vk, dst);
        vk_destroy_image(vk, src);

        vk_log("%s: quad: %d MB/s", bench_image_test_describe_mt(test, tiling, i, desc),
               bench_image_test_calc_throughput_mb(test, dur));
    }
}

static void
bench_image_test_draw(struct bench_image_test *test)
{
    bench_image_test_draw_clear(test, VK_IMAGE_TILING_LINEAR);
    bench_image_test_draw_clear(test, VK_IMAGE_TILING_OPTIMAL);

    bench_image_test_draw_copy(test, VK_IMAGE_TILING_LINEAR);
    bench_image_test_draw_copy(test, VK_IMAGE_TILING_OPTIMAL);

    bench_image_test_draw_compute(test, VK_IMAGE_TILING_LINEAR);
    bench_image_test_draw_compute(test, VK_IMAGE_TILING_OPTIMAL);

    bench_image_test_draw_quad(test, VK_IMAGE_TILING_LINEAR);
    bench_image_test_draw_quad(test, VK_IMAGE_TILING_OPTIMAL);
}

int
main(int argc, char **argv)
{
    struct bench_image_test test = {
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .elem_size = sizeof(float[4]),
        .width = 1920,
        .height = 1080,
        .loop = 32,

        .cs_local_size = 8,
    };

    bench_image_test_init(&test);
    bench_image_test_draw(&test);
    bench_image_test_cleanup(&test);

    return 0;
}
