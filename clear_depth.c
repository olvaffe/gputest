/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

struct clear_depth_test {
    VkFormat format;
    VkExtent2D size;
    VkClearDepthStencilValue clear_val;

    VkImageAspectFlags dump_aspect_mask;
    VkExtent2D dump_size;

    struct vk vk;

    struct vk_image *img;

    struct vk_buffer *buf;
    uint32_t depth_stride;
    uint32_t depth_size;
    uint32_t stencil_offset;
    uint32_t stencil_stride;
    uint32_t stencil_size;
};

static VkImageAspectFlags
clear_depth_test_get_aspect_mask(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_S8_UINT:
        return VK_IMAGE_ASPECT_STENCIL_BIT;
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
        vk_die("bad format");
    }
}

static uint32_t
clear_depth_test_get_cpp(VkFormat format, VkImageAspectFlagBits aspect)
{
    switch (format) {
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
        return 4;
    case VK_FORMAT_D16_UNORM:
        return 2;
    case VK_FORMAT_S8_UINT:
        return 1;
    case VK_FORMAT_D16_UNORM_S8_UINT:
        return aspect == VK_IMAGE_ASPECT_DEPTH_BIT ? 2 : 1;
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return aspect == VK_IMAGE_ASPECT_DEPTH_BIT ? 4 : 1;
    default:
        vk_die("bad format");
    }
}

static void
clear_depth_test_init(struct clear_depth_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);

    test->img =
        vk_create_image(vk, test->format, test->size.width, test->size.height,
                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    if (test->dump_aspect_mask & VK_IMAGE_ASPECT_DEPTH_BIT) {
        test->depth_stride = test->dump_size.width *
                             clear_depth_test_get_cpp(test->format, VK_IMAGE_ASPECT_DEPTH_BIT);
        test->depth_size = test->depth_stride * test->dump_size.height;
    }

    if (test->dump_aspect_mask & VK_IMAGE_ASPECT_STENCIL_BIT) {
        test->stencil_offset = test->depth_size;
        test->stencil_stride =
            test->dump_size.width *
            clear_depth_test_get_cpp(test->format, VK_IMAGE_ASPECT_STENCIL_BIT);
        test->stencil_size = test->stencil_stride * test->dump_size.height;
    }

    const VkDeviceSize buf_size = test->depth_size + test->stencil_size;
    test->buf = vk_create_buffer(vk, buf_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    memset(test->buf->mem_ptr, 0xaa, buf_size);
}

static void
clear_depth_test_cleanup(struct clear_depth_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_buffer(vk, test->buf);
    vk_destroy_image(vk, test->img);
    vk_cleanup(vk);
}

static void
clear_depth_test_copy(struct clear_depth_test *test, VkCommandBuffer cmd, VkImageLayout layout)
{
    struct vk *vk = &test->vk;

    VkBufferImageCopy regions[2];
    uint32_t region_count = 0;
    if (test->dump_aspect_mask & VK_IMAGE_ASPECT_DEPTH_BIT) {
        regions[region_count++] = (VkBufferImageCopy){
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .layerCount = 1,
            },
            .imageExtent = {
                .width = test->dump_size.width,
                .height = test->dump_size.height,
                .depth = 1,
            },
        };
    };
    if (test->dump_aspect_mask & VK_IMAGE_ASPECT_STENCIL_BIT) {
        regions[region_count++] = (VkBufferImageCopy){
            .bufferOffset = test->stencil_offset,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT,
                .layerCount = 1,
            },
            .imageExtent = {
                .width = test->dump_size.width,
                .height = test->dump_size.height,
                .depth = 1,
            },
        };
    };

    vk->CmdCopyImageToBuffer(cmd, test->img->img, layout, test->buf->buf, region_count, regions);

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
clear_depth_test_clear(struct clear_depth_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageSubresourceRange subres_range = {
        .aspectMask = clear_depth_test_get_aspect_mask(test->format),
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier barrier1 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = test->img->img,
        .subresourceRange = subres_range,
    };
    const VkImageMemoryBarrier barrier2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = test->img->img,
        .subresourceRange = subres_range,
    };
    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0, 0, NULL, 0, NULL, 1, &barrier1);

    vk->CmdClearDepthStencilImage(cmd, test->img->img, barrier1.newLayout, &test->clear_val, 1,
                                  &subres_range);

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                           0, NULL, 0, NULL, 1, &barrier2);

    clear_depth_test_copy(test, cmd, barrier2.newLayout);
}

static void
clear_depth_test_dump(struct clear_depth_test *test)
{
    struct vk *vk = &test->vk;

    if (test->dump_aspect_mask & VK_IMAGE_ASPECT_DEPTH_BIT) {
        vk_dump_buffer_raw(vk, test->buf, 0, test->depth_size, "rt.depth");

        for (uint32_t y = 0; y < test->dump_size.height; y++) {
            const union {
                const void *ptr;
                const uint16_t *u16;
                const uint32_t *u32;
                const float *f32;
            } row = { .ptr = test->buf->mem_ptr + test->depth_stride * y };

            for (uint32_t x = 0; x < test->dump_size.width; x++) {
                float v;

                switch (test->format) {
                case VK_FORMAT_D16_UNORM:
                case VK_FORMAT_D16_UNORM_S8_UINT:
                    v = (float)row.u16[x] / (float)((1 << 16) - 1);
                    break;
                case VK_FORMAT_X8_D24_UNORM_PACK32:
                case VK_FORMAT_D24_UNORM_S8_UINT:
                    v = (float)row.u32[x] / (float)(~0u);
                    break;
                case VK_FORMAT_D32_SFLOAT:
                case VK_FORMAT_D32_SFLOAT_S8_UINT:
                    v = row.f32[x];
                    break;
                default:
                    vk_die("bad format");
                }

                if (fabs(v - test->clear_val.depth) >= 0.01f)
                    vk_die("depth (%d, %d) is %f, not %f", x, y, v, test->clear_val.depth);
            }
        }
    }

    if (test->dump_aspect_mask & VK_IMAGE_ASPECT_STENCIL_BIT) {
        vk_dump_buffer_raw(vk, test->buf, test->stencil_offset, test->stencil_size, "rt.stencil");

        for (uint32_t y = 0; y < test->dump_size.height; y++) {
            const uint8_t *row =
                test->buf->mem_ptr + test->stencil_offset + test->stencil_stride * y;
            for (uint32_t x = 0; x < test->dump_size.width; x++) {
                if (row[x] != test->clear_val.stencil) {
                    vk_die("stencil (%d, %d) is %d, not %d", x, y, row[x],
                           test->clear_val.stencil);
                }
            }
        }
    }
}

static void
clear_depth_test_draw(struct clear_depth_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk);

    clear_depth_test_clear(test, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    clear_depth_test_dump(test);
}

int
main(void)
{
    struct clear_depth_test test = {
        .format = VK_FORMAT_D16_UNORM_S8_UINT,
        .size = {
            .width = 8,
            .height = 16,
        },
        .clear_val =  {
            .depth = 0.5f,
            .stencil = 8,
        },
        .dump_aspect_mask = VK_IMAGE_ASPECT_STENCIL_BIT,
    };

    test.dump_aspect_mask &= clear_depth_test_get_aspect_mask(test.format);
    test.dump_size = test.size;

    clear_depth_test_init(&test);
    clear_depth_test_draw(&test);
    clear_depth_test_cleanup(&test);

    return 0;
}
