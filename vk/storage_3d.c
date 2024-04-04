/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static const uint32_t storage_3d_test_cs[] = {
#include "storage_3d_test.comp.inc"
};

struct storage_3d_test_push_const {
    uint32_t level;
};

struct storage_3d_test {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    bool mipmapped;

    VkFormat img_format;
    VkFormat view_format;

    struct vk vk;
    struct vk_image *img;
    struct vk_buffer *buf;

    struct vk_pipeline *pipeline;
    struct vk_descriptor_set **sets;
    VkImageView *views;
};

static void
storage_3d_test_init_descriptor_sets(struct storage_3d_test *test)
{
    struct vk *vk = &test->vk;

    test->sets = malloc(sizeof(*test->sets) * test->img->info.mipLevels);
    test->views = malloc(sizeof(*test->views) * test->img->info.mipLevels);
    if (!test->sets || !test->views)
        vk_die("failed to alloc sets/views");

    for (uint32_t i = 0; i < test->img->info.mipLevels; i++) {
        const VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = test->img->img,
            .viewType = VK_IMAGE_VIEW_TYPE_3D,
            .format = test->view_format,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = i,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        vk->result = vk->CreateImageView(vk->dev, &view_info, NULL, &test->views[i]);
        vk_check(vk, "failed to create image view");

        test->sets[i] = vk_create_descriptor_set(vk, test->pipeline->set_layouts[0]);
        const VkDescriptorImageInfo img_info = {
            .imageView = test->views[i],
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };
        const VkWriteDescriptorSet write_info = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = test->sets[i]->set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &img_info,
        };
        vk->UpdateDescriptorSets(vk->dev, 1, &write_info, 0, NULL);
    }
}

static void
storage_3d_test_init_pipeline(struct storage_3d_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_COMPUTE_BIT, storage_3d_test_cs,
                           sizeof(storage_3d_test_cs));

    vk_add_pipeline_set_layout(vk, test->pipeline, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                               VK_SHADER_STAGE_COMPUTE_BIT, NULL);
    vk_set_pipeline_push_const(vk, test->pipeline, VK_SHADER_STAGE_COMPUTE_BIT,
                               sizeof(struct storage_3d_test_push_const));

    vk_setup_pipeline(vk, test->pipeline, NULL);
    vk_compile_pipeline(vk, test->pipeline);
}

static VkDeviceSize
storage_3d_test_get_miplevel_size(struct storage_3d_test *test, uint32_t level)
{
    uint32_t bpp;
    switch (test->img_format) {
    case VK_FORMAT_R8G8B8A8_UNORM:
        bpp = 4;
        break;
    default:
        vk_die("unsupported image format");
        break;
    }

    return u_minify(test->width, level) * u_minify(test->height, level) *
           u_minify(test->depth, level) * bpp;
}

static void
storage_3d_test_init_buffer(struct storage_3d_test *test)
{
    struct vk *vk = &test->vk;

    VkDeviceSize size = 0;
    for (uint32_t i = 0; i < test->img->info.mipLevels; i++)
        size += storage_3d_test_get_miplevel_size(test, i);

    test->buf = vk_create_buffer(vk, 0, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
}

static void
storage_3d_test_init_image(struct storage_3d_test *test)
{
    struct vk *vk = &test->vk;

    uint32_t level_count = 1;
    if (test->mipmapped) {
        uint32_t max = test->width;
        if (max < test->height)
            max = test->height;
        if (max < test->depth)
            max = test->depth;

        while (max != 1) {
            max = u_minify(max, 1);
            level_count++;
        }
    }

    vk_log("image size %dx%dx%d level count %d", test->width, test->height, test->depth,
           level_count);

    VkImageCreateFlags flags = 0;
    if (test->img_format != test->view_format)
        flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

    const VkImageCreateInfo img_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = flags,
        .imageType = VK_IMAGE_TYPE_3D,
        .format = test->img_format,
        .extent = {
            .width = test->width,
            .height = test->height,
            .depth = test->depth,
        },
        .mipLevels = level_count,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    test->img = vk_create_image_from_info(vk, &img_info);
}

static void
storage_3d_test_init(struct storage_3d_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);

    storage_3d_test_init_image(test);
    storage_3d_test_init_buffer(test);
    storage_3d_test_init_pipeline(test);
    storage_3d_test_init_descriptor_sets(test);
}

static void
storage_3d_test_cleanup(struct storage_3d_test *test)
{
    struct vk *vk = &test->vk;

    for (uint32_t i = 0; i < test->img->info.mipLevels; i++) {
        vk->DestroyImageView(vk->dev, test->views[i], NULL);
        vk_destroy_descriptor_set(vk, test->sets[i]);
    }
    free(test->views);
    free(test->sets);

    vk_destroy_pipeline(vk, test->pipeline);

    vk_destroy_buffer(vk, test->buf);
    vk_destroy_image(vk, test->img);

    vk_cleanup(vk);
}

static void
storage_3d_test_draw_quad(struct storage_3d_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = test->img->info.mipLevels,
        .layerCount = test->img->info.arrayLayers,
    };
    const VkImageMemoryBarrier barrier1 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = test->img->img,
        .subresourceRange = subres_range,
    };
    const VkImageMemoryBarrier barrier2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .image = test->img->img,
        .subresourceRange = subres_range,
    };

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1,
                           &barrier1);

    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, test->pipeline->pipeline);

    for (uint32_t i = 0; i < test->img->info.mipLevels; i++) {
        vk->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                  test->pipeline->pipeline_layout, 0, 1, &test->sets[i]->set, 0,
                                  NULL);

        const struct storage_3d_test_push_const push = {
            .level = i,
        };
        vk->CmdPushConstants(cmd, test->pipeline->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                             sizeof(push), &push);

        const uint32_t workgroup_size[3] = { 4, 4, 4 };
        const uint32_t workgroup_count[3] = {
            DIV_ROUND_UP(u_minify(test->width, i), workgroup_size[0]),
            DIV_ROUND_UP(u_minify(test->height, i), workgroup_size[1]),
            DIV_ROUND_UP(u_minify(test->depth, i), workgroup_size[2]),
        };
        vk->CmdDispatch(cmd, workgroup_count[0], workgroup_count[1], workgroup_count[2]);
    }

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier2);

    VkBufferImageCopy *copies = malloc(sizeof(*copies) * test->img->info.mipLevels);
    if (!copies)
        vk_die("failed to alloc copies");
    VkDeviceSize buf_offset = 0;
    for (uint32_t i = 0; i < test->img->info.mipLevels; i++) {
        copies[i] = (VkBufferImageCopy){
            .bufferOffset = buf_offset,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = i,
                .layerCount = test->img->info.arrayLayers,
            },
            .imageExtent = {
                .width = u_minify(test->width, i),
                .height = u_minify(test->height, i),
                .depth = u_minify(test->depth, i),
            },
        };
        buf_offset += storage_3d_test_get_miplevel_size(test, i);
    }
    vk->CmdCopyImageToBuffer(cmd, test->img->img, barrier2.newLayout, test->buf->buf,
                             test->img->info.mipLevels, copies);
    free(copies);

    const VkBufferMemoryBarrier buf_barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .buffer = test->buf->buf,
        .size = VK_WHOLE_SIZE,
    };
    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                           NULL, 1, &buf_barrier, 0, NULL);
}

static void
storage_3d_test_draw(struct storage_3d_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);
    storage_3d_test_draw_quad(test, cmd);
    vk_end_cmd(vk);
    vk_wait(vk);

    VkDeviceSize buf_offset = 0;
    for (uint32_t i = 0; i < test->img->info.mipLevels; i++) {
        const uint32_t mip_width = u_minify(test->width, i);
        const uint32_t mip_height = u_minify(test->height, i);
        const uint32_t mip_depth = u_minify(test->depth, i);

        const uint8_t *ptr = test->buf->mem_ptr + buf_offset;
        buf_offset += storage_3d_test_get_miplevel_size(test, i);

        for (uint32_t z = 0; z < mip_depth; z++) {
            for (uint32_t y = 0; y < mip_height; y++) {
                for (uint32_t x = 0; x < mip_width; x++) {
                    const uint8_t val[4] = { x & 0xff, y & 0xff, z & 0xff, i & 0xff };
                    if (memcmp(ptr, val, sizeof(val))) {
                        vk_die("(%d, %d, %d, %d) is (%d, %d, %d, %d), not (%d, %d, %d, %d)", x, y,
                               z, i, ptr[0], ptr[1], ptr[2], ptr[3], val[0], val[1], val[2],
                               val[3]);
                    }
                    ptr += ARRAY_SIZE(val);
                }
            }
        }
    }
}

int
main(int argc, char **argv)
{
    struct storage_3d_test test = {
        .width = 128,
        .height = 64,
        .depth = 8,
        .mipmapped = true,
        .img_format = VK_FORMAT_R8G8B8A8_UNORM,
        .view_format = VK_FORMAT_R8G8B8A8_UINT,
    };

    if (argc != 1 && argc != 5) {
        vk_log("Usage: %s [width height depth mipmapped]", argv[0]);
        return -1;
    }

    if (argc == 5) {
        test.width = atoi(argv[1]);
        test.height = atoi(argv[2]);
        test.depth = atoi(argv[3]);
        test.mipmapped = atoi(argv[4]);
    }

    storage_3d_test_init(&test);
    storage_3d_test_draw(&test);
    storage_3d_test_cleanup(&test);

    return 0;
}
