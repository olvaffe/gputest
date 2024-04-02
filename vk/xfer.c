/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

struct xfer_test {
    struct vk vk;

    bool verbose;
    VkDeviceSize buf_size;
    uint32_t img_width;
    uint32_t img_height;

    VkCommandBuffer cmd;
    struct vk_buffer *bufs[4];
    uint32_t buf_count;
    struct vk_image *imgs[4];
    uint32_t img_count;
};

struct xfer_test_format {
    VkFormat format;
    const char *name;

    bool color;
    bool depth;
    bool stencil;
    bool compressed;
    bool ycbcr;
    uint32_t plane_count;

    VkFormatProperties2 props;
};

static struct xfer_test_format xfer_test_formats[] = {
#define FMT_COMMON(fmt) .format = VK_FORMAT_##fmt, .name = #fmt

#define FMT(fmt)                                                                                 \
    {                                                                                            \
        FMT_COMMON(fmt),                                                                         \
        .color = true,                                                                           \
        .plane_count = 1,                                                                        \
    },
#define FMT_D(fmt)                                                                               \
    {                                                                                            \
        FMT_COMMON(fmt),                                                                         \
        .depth = true,                                                                           \
        .plane_count = 1,                                                                        \
    },
#define FMT_S(fmt)                                                                               \
    {                                                                                            \
        FMT_COMMON(fmt),                                                                         \
        .stencil = true,                                                                         \
        .plane_count = 1,                                                                        \
    },
#define FMT_DS(fmt)                                                                              \
    {                                                                                            \
        FMT_COMMON(fmt),                                                                         \
        .depth = true,                                                                           \
        .stencil = true,                                                                         \
        .plane_count = 1,                                                                        \
    },
#define FMT_COMPRESSED(fmt)                                                                      \
    {                                                                                            \
        FMT_COMMON(fmt),                                                                         \
        .color = true,                                                                           \
        .compressed = true,                                                                      \
        .plane_count = 1,                                                                        \
    },
#define FMT_YCBCR(fmt)                                                                           \
    {                                                                                            \
        FMT_COMMON(fmt),                                                                         \
        .color = true,                                                                           \
        .ycbcr = true,                                                                           \
        .plane_count = 1,                                                                        \
    },
#define FMT_2PLANE(fmt)                                                                          \
    {                                                                                            \
        FMT_COMMON(fmt),                                                                         \
        .color = true,                                                                           \
        .ycbcr = true,                                                                           \
        .plane_count = 2,                                                                        \
    },
#define FMT_3PLANE(fmt)                                                                          \
    {                                                                                            \
        FMT_COMMON(fmt),                                                                         \
        .color = true,                                                                           \
        .ycbcr = true,                                                                           \
        .plane_count = 3,                                                                        \
    },
#include "vkutil_formats.inc"

#undef FMT_COMMON
};

static void
xfer_test_init_formats(struct xfer_test *test)
{
    struct vk *vk = &test->vk;

    for (uint32_t i = 0; i < ARRAY_SIZE(xfer_test_formats); i++) {
        struct xfer_test_format *fmt = &xfer_test_formats[i];

        fmt->props.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
        vk->GetPhysicalDeviceFormatProperties2(vk->physical_dev, fmt->format, &fmt->props);
    }
}

static void
xfer_test_init(struct xfer_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);
    xfer_test_init_formats(test);
}

static void
xfer_test_cleanup(struct xfer_test *test)
{
    struct vk *vk = &test->vk;

    vk_cleanup(vk);
}

static VkCommandBuffer
xfer_test_begin_cmd(struct xfer_test *test)
{
    struct vk *vk = &test->vk;
    test->cmd = vk_begin_cmd(vk, false);
    return test->cmd;
}

static struct vk_buffer *
xfer_test_begin_buffer(struct xfer_test *test, VkBufferUsageFlags usage)
{
    struct vk *vk = &test->vk;

    if (!test->cmd)
        vk_die("no cmd");

    if (test->buf_count >= ARRAY_SIZE(test->bufs))
        vk_die("too many buffers");

    struct vk_buffer *buf = vk_create_buffer(vk, 0, test->buf_size, usage);
    test->bufs[test->buf_count++] = buf;
    return buf;
}

static struct vk_image *
xfer_test_begin_image(struct xfer_test *test,
                      const struct xfer_test_format *fmt,
                      VkSampleCountFlagBits samples,
                      VkImageTiling tiling,
                      VkImageUsageFlags usage,
                      VkImageLayout layout)
{
    struct vk *vk = &test->vk;

    if (!test->cmd)
        vk_die("no cmd");

    if (test->img_count >= ARRAY_SIZE(test->imgs))
        vk_die("too many images");

    struct vk_image *img = vk_create_image(vk, fmt->format, test->img_width, test->img_height,
                                           samples, tiling, usage);

    VkImageAspectFlags aspect_mask = 0;
    if (fmt->color)
        aspect_mask |= VK_IMAGE_ASPECT_COLOR_BIT;
    if (fmt->depth)
        aspect_mask |= VK_IMAGE_ASPECT_DEPTH_BIT;
    if (fmt->stencil)
        aspect_mask |= VK_IMAGE_ASPECT_STENCIL_BIT;

    const VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = layout,
        .image = img->img,
        .subresourceRange = {
            .aspectMask = aspect_mask,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    vk->CmdPipelineBarrier(test->cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    test->imgs[test->img_count++] = img;
    return img;
}

static void
xfer_test_end_all(struct xfer_test *test)
{
    struct vk *vk = &test->vk;

    vk_end_cmd(vk);
    vk_wait(vk);

    test->cmd = NULL;

    for (uint32_t i = 0; i < test->buf_count; i++)
        vk_destroy_buffer(vk, test->bufs[i]);
    test->buf_count = 0;

    for (uint32_t i = 0; i < test->img_count; i++)
        vk_destroy_image(vk, test->imgs[i]);
    test->img_count = 0;
}

static void
xfer_test_draw_fill_buffer(struct xfer_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = xfer_test_begin_cmd(test);
    struct vk_buffer *buf = xfer_test_begin_buffer(test, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    vk->CmdFillBuffer(cmd, buf->buf, 0, VK_WHOLE_SIZE, 0x37);

    xfer_test_end_all(test);
}

static void
xfer_test_draw_update_buffer(struct xfer_test *test)
{
    struct vk *vk = &test->vk;
    const uint32_t data[] = { 0x37, 0x38, 0x39, 0x40 };

    VkCommandBuffer cmd = xfer_test_begin_cmd(test);
    struct vk_buffer *buf = xfer_test_begin_buffer(test, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    vk->CmdUpdateBuffer(cmd, buf->buf, 0, ARRAY_SIZE(data), data);

    xfer_test_end_all(test);
}

static void
xfer_test_draw_copy_buffer(struct xfer_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = xfer_test_begin_cmd(test);
    struct vk_buffer *buf = xfer_test_begin_buffer(
        test, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    const VkDeviceSize size = buf->info.size / 2;
    const VkBufferCopy region = {
        .srcOffset = 0,
        .dstOffset = size,
        .size = size,
    };
    vk->CmdCopyBuffer(cmd, buf->buf, buf->buf, 1, &region);

    xfer_test_end_all(test);
}

static VkExtent3D
xfer_test_get_copy_extent(struct xfer_test *test, const struct xfer_test_format *fmt)
{
    VkExtent3D extent = {
        .width = 8,
        .height = 8,
        .depth = 1,
    };

    switch (fmt->format) {
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK:
        extent.width = 5;
        extent.height = 4;
        break;
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK:
        extent.width = 5;
        extent.height = 5;
        break;
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK:
        extent.width = 6;
        extent.height = 5;
        break;
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK:
        extent.width = 6;
        extent.height = 6;
        break;
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK:
        extent.width = 8;
        extent.height = 5;
        break;
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK:
        extent.width = 8;
        extent.height = 6;
        break;
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK:
        extent.width = 10;
        extent.height = 5;
        break;
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK:
        extent.width = 10;
        extent.height = 6;
        break;
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK:
        extent.width = 10;
        extent.height = 8;
        break;
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK:
        extent.width = 10;
        extent.height = 10;
        break;
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK:
        extent.width = 12;
        extent.height = 10;
        break;
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK:
        extent.width = 12;
        extent.height = 12;
        break;
    default:
        break;
    }

    return extent;
}

static uint32_t
xfer_test_get_buffer_image_copy(struct xfer_test *test,
                                const struct xfer_test_format *fmt,
                                VkBufferImageCopy regions[static 4])
{
    const VkExtent3D extent = xfer_test_get_copy_extent(test, fmt);

    /* VUID-VkBufferImageCopy-aspectMask-00212
     * The aspectMask member of imageSubresource must only have a single bit
     * set
     */

    uint32_t region_count = 0;

    if (fmt->color) {
        for (uint32_t i = 0; i < fmt->plane_count; i++) {
            const VkImageAspectFlagBits aspect = fmt->plane_count == 1 ? VK_IMAGE_ASPECT_COLOR_BIT
                                                 : i == 0 ? VK_IMAGE_ASPECT_PLANE_0_BIT
                                                 : i == 1 ? VK_IMAGE_ASPECT_PLANE_1_BIT
                                                          : VK_IMAGE_ASPECT_PLANE_2_BIT;

            regions[i] = (VkBufferImageCopy){
                .imageSubresource = {
                    .aspectMask = aspect,
                    .layerCount = 1,
                },
                .imageExtent = extent,
            };
        }

        region_count = fmt->plane_count;
    }
    if (fmt->depth) {
        regions[region_count++] = (VkBufferImageCopy){
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .layerCount = 1,
            },
                .imageExtent = extent,
        };
    }
    if (fmt->stencil) {
        regions[region_count++] = (VkBufferImageCopy){
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT,
                .layerCount = 1,
            },
                .imageExtent = extent,
        };
    }

    return region_count;
}

static void
xfer_test_draw_copy_image_to_buffer(struct xfer_test *test,
                                    const struct xfer_test_format *fmt,
                                    VkImageTiling tiling)
{
    struct vk *vk = &test->vk;

    if (test->verbose)
        vk_log("  copy %s image to buffer", tiling ? "linear" : "optimal");

    VkBufferImageCopy regions[4];
    const uint32_t region_count = xfer_test_get_buffer_image_copy(test, fmt, regions);

    VkCommandBuffer cmd = xfer_test_begin_cmd(test);
    struct vk_image *img = xfer_test_begin_image(test, fmt, VK_SAMPLE_COUNT_1_BIT, tiling,
                                                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    struct vk_buffer *buf = xfer_test_begin_buffer(test, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    vk->CmdCopyImageToBuffer(cmd, img->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf->buf,
                             region_count, regions);

    xfer_test_end_all(test);
}

static void
xfer_test_draw_copy_buffer_to_image(struct xfer_test *test,
                                    const struct xfer_test_format *fmt,
                                    VkImageTiling tiling)
{
    struct vk *vk = &test->vk;

    if (test->verbose)
        vk_log("  copy buffer to %s image", tiling ? "linear" : "optimal");

    VkBufferImageCopy regions[4];
    const uint32_t region_count = xfer_test_get_buffer_image_copy(test, fmt, regions);

    VkCommandBuffer cmd = xfer_test_begin_cmd(test);
    struct vk_buffer *buf = xfer_test_begin_buffer(test, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    struct vk_image *img = xfer_test_begin_image(test, fmt, VK_SAMPLE_COUNT_1_BIT, tiling,
                                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vk->CmdCopyBufferToImage(cmd, buf->buf, img->img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             region_count, regions);

    xfer_test_end_all(test);
}

static void
xfer_test_draw_clear_color_image(struct xfer_test *test,
                                 const struct xfer_test_format *fmt,
                                 VkImageTiling tiling)
{
    struct vk *vk = &test->vk;

    /* VUID-vkCmdClearColorImage-image-00007
     * image must not have a compressed or depth/stencil format
     *
     * VUID-vkCmdClearColorImage-image-01545
     * image must not use any of the formats that require a sampler Y′CBCR
     * conversion
     */
    if (!fmt->color || fmt->compressed || fmt->ycbcr)
        return;

    if (test->verbose)
        vk_log("  clear %s color image", tiling ? "linear" : "optimal");

    /* VUID-vkCmdClearColorImage-aspectMask-02498
     * The VkImageSubresourceRange::aspectMask members of the elements of the
     * pRanges array must each only include VK_IMAGE_ASPECT_COLOR_BIT
     */
    const VkImageSubresourceRange region = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };

    const VkClearColorValue clear = { 0 };

    VkCommandBuffer cmd = xfer_test_begin_cmd(test);
    struct vk_image *img = xfer_test_begin_image(test, fmt, VK_SAMPLE_COUNT_1_BIT, tiling,
                                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vk->CmdClearColorImage(cmd, img->img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1,
                           &region);

    xfer_test_end_all(test);
}

static void
xfer_test_draw_clear_depth_stencil_image(struct xfer_test *test,
                                         const struct xfer_test_format *fmt,
                                         VkImageTiling tiling)
{
    struct vk *vk = &test->vk;

    /* VUID-vkCmdClearDepthStencilImage-image-00014
     * image must have a depth/stencil format
     */
    if (!fmt->depth && !fmt->stencil)
        return;

    if (test->verbose)
        vk_log("  clear %s depth/stencil image", tiling ? "linear" : "optimal");

    /* VUID-vkCmdClearDepthStencilImage-image-02825
     * If the image’s format does not have a stencil component, then the
     * VkImageSubresourceRange::aspectMask member of each element of the
     * pRanges array must not include the VK_IMAGE_ASPECT_STENCIL_BIT bit
     *
     * VUID-vkCmdClearDepthStencilImage-image-02826
     * If the image’s format does not have a depth component, then the
     * VkImageSubresourceRange::aspectMask member of each element of the
     * pRanges array must not include the VK_IMAGE_ASPECT_DEPTH_BIT bit
     */
    VkImageSubresourceRange regions[4];
    uint32_t region_count = 0;
    if (fmt->depth) {
        regions[region_count++] = (VkImageSubresourceRange){
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .levelCount = 1,
            .layerCount = 1,
        };
    };
    if (fmt->stencil) {
        regions[region_count++] = (VkImageSubresourceRange){
            .aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT,
            .levelCount = 1,
            .layerCount = 1,
        };
    };
    if (fmt->depth && fmt->stencil) {
        regions[region_count++] = (VkImageSubresourceRange){
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
            .levelCount = 1,
            .layerCount = 1,
        };
    };

    const VkClearDepthStencilValue clear = {
        .depth = 1.0f,
        .stencil = 0,
    };

    VkCommandBuffer cmd = xfer_test_begin_cmd(test);
    struct vk_image *img = xfer_test_begin_image(test, fmt, VK_SAMPLE_COUNT_1_BIT, tiling,
                                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vk->CmdClearDepthStencilImage(cmd, img->img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear,
                                  region_count, regions);

    xfer_test_end_all(test);
}

static uint32_t
xfer_test_get_image_copy(struct xfer_test *test,
                         const struct xfer_test_format *src_fmt,
                         const struct xfer_test_format *dst_fmt,
                         VkImageCopy regions[static 4])
{
    /* VUID-vkCmdCopyImage-srcImage-01548
     * If the VkFormat of each of srcImage and dstImage is not a
     * multi-planar format, the VkFormat of each of srcImage and dstImage
     * must be size-compatible
     *
     * VUID-vkCmdCopyImage-None-01549
     * In a copy to or from a plane of a multi-planar image, the VkFormat
     * of the image and plane must be compatible according to the
     * description of compatible planes for the plane being copied
     *
     * TODO do not require the same format
     */
    if (src_fmt != dst_fmt)
        return 0;

    const VkExtent3D extent = xfer_test_get_copy_extent(test, src_fmt);

    uint32_t region_count = 0;

    if (src_fmt->color) {
        for (uint32_t i = 0; i < src_fmt->plane_count; i++) {
            const VkImageAspectFlagBits aspect = src_fmt->plane_count == 1
                                                     ? VK_IMAGE_ASPECT_COLOR_BIT
                                                 : i == 0 ? VK_IMAGE_ASPECT_PLANE_0_BIT
                                                 : i == 1 ? VK_IMAGE_ASPECT_PLANE_1_BIT
                                                          : VK_IMAGE_ASPECT_PLANE_2_BIT;

            regions[i] = (VkImageCopy){
                .srcSubresource = {
                    .aspectMask = aspect,
                    .layerCount = 1,
                },
                .dstSubresource = {
                    .aspectMask = aspect,
                    .layerCount = 1,
                },
                .extent = extent,
            };
        }

        region_count = src_fmt->plane_count;
    }
    if (src_fmt->depth) {
        regions[region_count++] = (VkImageCopy){
            .srcSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .layerCount = 1,
            },
            .dstSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .layerCount = 1,
            },
                .extent = extent,
        };
    }
    if (src_fmt->stencil) {
        regions[region_count++] = (VkImageCopy){
            .srcSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT,
                .layerCount = 1,
            },
            .dstSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT,
                .layerCount = 1,
            },
                .extent = extent,
        };
    }

    /* unlike VkBufferImageCopy, depth and stencil can be copied at the same
     * time
     */
    if (src_fmt->depth && src_fmt->stencil) {
        regions[region_count++] = (VkImageCopy){
            .srcSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                .layerCount = 1,
            },
            .dstSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                .layerCount = 1,
            },
                .extent = extent,
        };
    }

    return region_count;
}

static void
xfer_test_draw_copy_image(struct xfer_test *test,
                          const struct xfer_test_format *src_fmt,
                          VkImageTiling src_tiling,
                          const struct xfer_test_format *dst_fmt,
                          VkImageTiling dst_tiling)
{
    struct vk *vk = &test->vk;

    VkImageCopy regions[4];
    const uint32_t region_count = xfer_test_get_image_copy(test, src_fmt, dst_fmt, regions);
    if (!region_count)
        return;

    if (test->verbose) {
        vk_log("  copy %s image to %s image", src_tiling ? "linear" : "optimal",
               dst_tiling ? "linear" : "optimal");
    }

    VkCommandBuffer cmd = xfer_test_begin_cmd(test);
    struct vk_image *src_img = xfer_test_begin_image(test, src_fmt, VK_SAMPLE_COUNT_1_BIT,
                                                     src_tiling, VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    struct vk_image *dst_img = xfer_test_begin_image(test, dst_fmt, VK_SAMPLE_COUNT_1_BIT,
                                                     dst_tiling, VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vk->CmdCopyImage(cmd, src_img->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_img->img,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, region_count, regions);

    xfer_test_end_all(test);
}

static uint32_t
xfer_test_get_image_blit(struct xfer_test *test,
                         const struct xfer_test_format *src_fmt,
                         const struct xfer_test_format *dst_fmt,
                         VkImageBlit regions[static 4])
{
    /* VUID-vkCmdBlitImage-srcImage-06421
     * srcImage must not use a format that requires a sampler Y′CBCR
     * conversion
     *
     * VUID-vkCmdBlitImage-dstImage-06422
     * dstImage must not use a format that requires a sampler Y′CBCR
     * conversion
     */
    if (src_fmt->ycbcr || dst_fmt->ycbcr)
        return 0;
    if (src_fmt->plane_count != 1 || dst_fmt->plane_count != 1)
        vk_die("non-ycbcr planar format?");

    /* VUID-vkCmdBlitImage-srcImage-00229
     * If either of srcImage or dstImage was created with a signed integer
     * VkFormat, the other must also have been created with a signed integer
     * VkFormat
     *
     * VUID-vkCmdBlitImage-srcImage-00231
     * If either of srcImage or dstImage was created with a depth/stencil
     * format, the other must have exactly the same format
     *
     * TODO do not require the same format
     */
    if (src_fmt != dst_fmt)
        return 0;

    const VkOffset3D src_end = {
        .x = 8,
        .y = 8,
        .z = 1,
    };
    const VkOffset3D dst_end = {
        .x = 16,
        .y = 16,
        .z = 1,
    };

    uint32_t region_count = 0;
    /* VUID-VkImageBlit-aspectMask-00238
     * The aspectMask member of srcSubresource and dstSubresource must match
     */
    if (src_fmt->color) {
        regions[region_count++] = (VkImageBlit){
                .srcSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .layerCount = 1,
                },
                .srcOffsets[1] = src_end,
                .dstSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .layerCount = 1,
                },
                .dstOffsets[1] = dst_end,
            };
    }
    if (src_fmt->depth) {
        regions[region_count++] = (VkImageBlit){
                .srcSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                    .layerCount = 1,
                },
                .srcOffsets[1] = src_end,
                .dstSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                    .layerCount = 1,
                },
                .dstOffsets[1] = dst_end,
            };
    }
    if (src_fmt->stencil) {
        regions[region_count++] = (VkImageBlit){
                .srcSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT,
                    .layerCount = 1,
                },
                .srcOffsets[1] = src_end,
                .dstSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT,
                    .layerCount = 1,
                },
                .dstOffsets[1] = dst_end,
            };
    }
    if (src_fmt->depth && src_fmt->stencil) {
        regions[region_count++] = (VkImageBlit){
                .srcSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                    .layerCount = 1,
                },
                .srcOffsets[1] = src_end,
                .dstSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                    .layerCount = 1,
                },
                .dstOffsets[1] = dst_end,
            };
    }

    return region_count;
}

static void
xfer_test_draw_blit_image(struct xfer_test *test,
                          const struct xfer_test_format *src_fmt,
                          VkImageTiling src_tiling,
                          const struct xfer_test_format *dst_fmt,
                          VkImageTiling dst_tiling)
{
    struct vk *vk = &test->vk;

    VkImageBlit regions[4];
    const uint32_t region_count = xfer_test_get_image_blit(test, src_fmt, dst_fmt, regions);
    if (!region_count)
        return;

    if (test->verbose) {
        vk_log("  blit %s image to %s image", src_tiling ? "linear" : "optimal",
               dst_tiling ? "linear" : "optimal");
    }

    /* VUID-vkCmdBlitImage-srcImage-00232
     * If srcImage was created with a depth/stencil format, filter must be
     * VK_FILTER_NEAREST
     *
     * VUID-vkCmdBlitImage-filter-02001
     * If filter is VK_FILTER_LINEAR, then the format features of srcImage
     * must contain VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT
     */
    const VkFilter filter = VK_FILTER_NEAREST;

    VkCommandBuffer cmd = xfer_test_begin_cmd(test);
    struct vk_image *src_img = xfer_test_begin_image(test, src_fmt, VK_SAMPLE_COUNT_1_BIT,
                                                     src_tiling, VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    struct vk_image *dst_img = xfer_test_begin_image(test, dst_fmt, VK_SAMPLE_COUNT_1_BIT,
                                                     dst_tiling, VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vk->CmdBlitImage(cmd, src_img->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_img->img,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, region_count, regions, filter);

    xfer_test_end_all(test);
}

static void
xfer_test_draw_resolve_image(struct xfer_test *test,
                             const struct xfer_test_format *fmt,
                             VkImageTiling tiling)
{
    struct vk *vk = &test->vk;

    /* check msaa support */
    const VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_4_BIT;
    VkImageFormatProperties img_props;
    const VkResult result = vk->GetPhysicalDeviceImageFormatProperties(
        vk->physical_dev, fmt->format, VK_IMAGE_TYPE_2D, tiling, VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        0, &img_props);
    if (result != VK_SUCCESS || !(img_props.sampleCounts & samples))
        return;

    if (test->verbose)
        vk_log("  resolve %s image", tiling ? "linear" : "optimal");

    VkCommandBuffer cmd = xfer_test_begin_cmd(test);
    struct vk_image *src_img =
        xfer_test_begin_image(test, fmt, samples, tiling, VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    struct vk_image *dst_img = xfer_test_begin_image(test, fmt, VK_SAMPLE_COUNT_1_BIT, tiling,
                                                     VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    /* VUID-VkImageResolve-aspectMask-00266
     * The aspectMask member of srcSubresource and dstSubresource must only
     * contain VK_IMAGE_ASPECT_COLOR_BIT
     */
    const VkImageResolve region = {
        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .extent = {
            .width = 8,
            .height = 8,
            .depth = 1,
        },
    };

    vk->CmdResolveImage(cmd, src_img->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_img->img,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    xfer_test_end_all(test);
}

static void
xfer_test_draw(struct xfer_test *test)
{
    vk_log("fill buffer");
    xfer_test_draw_fill_buffer(test);
    vk_log("update buffer");
    xfer_test_draw_update_buffer(test);
    vk_log("copy buffer");
    xfer_test_draw_copy_buffer(test);

    for (uint32_t i = 0; i < ARRAY_SIZE(xfer_test_formats); i++) {
        const struct xfer_test_format *fmt = &xfer_test_formats[i];
        const VkFormatFeatureFlags linear = fmt->props.formatProperties.linearTilingFeatures;
        const VkFormatFeatureFlags optimal = fmt->props.formatProperties.optimalTilingFeatures;
        const uint32_t xfer_bits =
            VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT |
            VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;

        if (!((linear | optimal) & xfer_bits))
            continue;

        vk_log("%s", fmt->name);

        /* vkCmdCopyImageToBuffer */
        if (linear & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT)
            xfer_test_draw_copy_image_to_buffer(test, fmt, VK_IMAGE_TILING_LINEAR);
        if (optimal & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT)
            xfer_test_draw_copy_image_to_buffer(test, fmt, VK_IMAGE_TILING_OPTIMAL);

        /* vkCmdCopyBufferToImage */
        if (linear & VK_FORMAT_FEATURE_TRANSFER_DST_BIT)
            xfer_test_draw_copy_buffer_to_image(test, fmt, VK_IMAGE_TILING_LINEAR);
        if (optimal & VK_FORMAT_FEATURE_TRANSFER_DST_BIT)
            xfer_test_draw_copy_buffer_to_image(test, fmt, VK_IMAGE_TILING_OPTIMAL);

        /* vkCmdClearColorImage */
        if (linear & VK_FORMAT_FEATURE_TRANSFER_DST_BIT)
            xfer_test_draw_clear_color_image(test, fmt, VK_IMAGE_TILING_LINEAR);
        if (optimal & VK_FORMAT_FEATURE_TRANSFER_DST_BIT)
            xfer_test_draw_clear_color_image(test, fmt, VK_IMAGE_TILING_OPTIMAL);

        /* vkCmdClearDepthStencilImage */
        if (linear & VK_FORMAT_FEATURE_TRANSFER_DST_BIT)
            xfer_test_draw_clear_depth_stencil_image(test, fmt, VK_IMAGE_TILING_LINEAR);
        if (optimal & VK_FORMAT_FEATURE_TRANSFER_DST_BIT)
            xfer_test_draw_clear_depth_stencil_image(test, fmt, VK_IMAGE_TILING_OPTIMAL);

        /* vkCmdCopyImage */
        if ((linear | optimal) & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) {
            for (uint32_t j = 0; j < ARRAY_SIZE(xfer_test_formats); j++) {
                const struct xfer_test_format *dst_fmt = &xfer_test_formats[j];
                const VkFormatFeatureFlags dst_linear =
                    dst_fmt->props.formatProperties.linearTilingFeatures;
                const VkFormatFeatureFlags dst_optimal =
                    dst_fmt->props.formatProperties.optimalTilingFeatures;

                if (linear & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) {
                    if (dst_linear & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) {
                        xfer_test_draw_copy_image(test, fmt, VK_IMAGE_TILING_LINEAR, dst_fmt,
                                                  VK_IMAGE_TILING_LINEAR);
                    }
                    if (dst_optimal & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) {
                        xfer_test_draw_copy_image(test, fmt, VK_IMAGE_TILING_LINEAR, dst_fmt,
                                                  VK_IMAGE_TILING_OPTIMAL);
                    }
                }

                if (optimal & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) {
                    if (dst_linear & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) {
                        xfer_test_draw_copy_image(test, fmt, VK_IMAGE_TILING_OPTIMAL, dst_fmt,
                                                  VK_IMAGE_TILING_LINEAR);
                    }
                    if (dst_optimal & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) {
                        xfer_test_draw_copy_image(test, fmt, VK_IMAGE_TILING_OPTIMAL, dst_fmt,
                                                  VK_IMAGE_TILING_OPTIMAL);
                    }
                }
            }
        }

        /* vkCmdBlitImage */
        if ((linear | optimal) & VK_FORMAT_FEATURE_BLIT_SRC_BIT) {
            for (uint32_t j = 0; j < ARRAY_SIZE(xfer_test_formats); j++) {
                const struct xfer_test_format *dst_fmt = &xfer_test_formats[j];
                const VkFormatFeatureFlags dst_linear =
                    dst_fmt->props.formatProperties.linearTilingFeatures;
                const VkFormatFeatureFlags dst_optimal =
                    dst_fmt->props.formatProperties.optimalTilingFeatures;

                if (linear & VK_FORMAT_FEATURE_BLIT_SRC_BIT) {
                    if (dst_linear & VK_FORMAT_FEATURE_BLIT_DST_BIT) {
                        xfer_test_draw_blit_image(test, fmt, VK_IMAGE_TILING_LINEAR, dst_fmt,
                                                  VK_IMAGE_TILING_LINEAR);
                    }
                    if (dst_optimal & VK_FORMAT_FEATURE_BLIT_DST_BIT) {
                        xfer_test_draw_blit_image(test, fmt, VK_IMAGE_TILING_LINEAR, dst_fmt,
                                                  VK_IMAGE_TILING_OPTIMAL);
                    }
                }

                if (optimal & VK_FORMAT_FEATURE_BLIT_SRC_BIT) {
                    if (dst_linear & VK_FORMAT_FEATURE_BLIT_DST_BIT) {
                        xfer_test_draw_blit_image(test, fmt, VK_IMAGE_TILING_OPTIMAL, dst_fmt,
                                                  VK_IMAGE_TILING_LINEAR);
                    }
                    if (dst_optimal & VK_FORMAT_FEATURE_BLIT_DST_BIT) {
                        xfer_test_draw_blit_image(test, fmt, VK_IMAGE_TILING_OPTIMAL, dst_fmt,
                                                  VK_IMAGE_TILING_OPTIMAL);
                    }
                }
            }
        }

        /* VUID-vkCmdResolveImage-dstImage-02003
         * The format features of dstImage must contain
         * VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
         *
         * VUID-vkCmdResolveImage-srcImage-01386
         * srcImage and dstImage must have been created with the same image
         * format
         *
         * VUID-vkCmdResolveImage-srcImage-06763
         * The format features of srcImage must contain
         * VK_FORMAT_FEATURE_TRANSFER_SRC_BIT
         *
         * VUID-vkCmdResolveImage-dstImage-06765
         * The format features of dstImage must contain
         * VK_FORMAT_FEATURE_TRANSFER_DST_BIT
         */
        const uint32_t resolve_bits = VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                                      VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
                                      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;

        /* vkCmdResolveImage */
        if ((linear & resolve_bits) == resolve_bits)
            xfer_test_draw_resolve_image(test, fmt, VK_IMAGE_TILING_LINEAR);
        if ((optimal & resolve_bits) == resolve_bits)
            xfer_test_draw_resolve_image(test, fmt, VK_IMAGE_TILING_OPTIMAL);
    }
}

int
main(void)
{
    struct xfer_test test = {
        .verbose = true,
        .buf_size = 4096,
        .img_width = 32,
        .img_height = 32,
    };

    xfer_test_init(&test);
    xfer_test_draw(&test);
    xfer_test_cleanup(&test);

    return 0;
}
