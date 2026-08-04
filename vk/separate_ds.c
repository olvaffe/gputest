/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static const uint32_t separate_ds_test_vs[] = {
#include "separate_ds_test.vert.inc"
};

struct separate_ds_test {
    VkFormat depth_format;
    VkImageLayout depth_layout;
    VkImageLayout stencil_layout;
    uint32_t width;
    uint32_t height;

    uint32_t depth_bits;
    uint32_t stencil_bits;
    VkImageAspectFlags aspect_mask;

    struct vk vk;

    struct vk_image *ds;
    VkRenderingAttachmentInfo depth_att;
    VkRenderingAttachmentInfo stencil_att;
    VkRenderingInfo rendering_info;

    struct vk_pipeline *pipeline;

    struct vk_buffer *d_buf;
    struct vk_buffer *s_buf;
};

static void
separate_ds_test_init_buffers(struct separate_ds_test *test)
{
    struct vk *vk = &test->vk;

    if (test->depth_bits) {
        VkDeviceSize size = test->width * test->height;
        size *= (test->depth_bits == 24 ? 32 : test->depth_bits) / 8;
        test->d_buf = vk_create_buffer(vk, 0, size, VK_BUFFER_USAGE_2_TRANSFER_DST_BIT);
    }

    if (test->stencil_bits) {
        const VkDeviceSize size = test->width * test->height * test->stencil_bits / 8;
        test->s_buf = vk_create_buffer(vk, 0, size, VK_BUFFER_USAGE_2_TRANSFER_DST_BIT);
    }
}

static void
separate_ds_test_init_pipeline(struct separate_ds_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, separate_ds_test_vs,
                           sizeof(separate_ds_test_vs));

    vk_set_pipeline_topology(vk, test->pipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

    vk_set_pipeline_viewport(vk, test->pipeline, test->width, test->height);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_FILL, false);

    vk_set_pipeline_sample_count(vk, test->pipeline, VK_SAMPLE_COUNT_1_BIT);

    test->pipeline->depth_info = (VkPipelineDepthStencilStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        /* depth test is silently skipped if depth_bits == 0 */
        .depthTestEnable = true,
        .depthWriteEnable = true,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        /* depth test is silently skipped if stencil_bits == 0 */
        .stencilTestEnable = true,
        .front = {
            .failOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP,
            .passOp = VK_STENCIL_OP_REPLACE,
            .depthFailOp = VK_STENCIL_OP_ZERO,
            .compareOp = VK_COMPARE_OP_LESS,
            .compareMask = 0xff,
            .writeMask = 0xff,
            .reference = 20,
        },
    };
    test->pipeline->rendering_info = (VkPipelineRenderingCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .depthAttachmentFormat = test->depth_bits ? test->depth_format : VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = test->stencil_bits ? test->depth_format : VK_FORMAT_UNDEFINED,
    };

    vk_compile_pipeline(vk, test->pipeline);
}

static void
separate_ds_test_init_image(struct separate_ds_test *test)
{
    struct vk *vk = &test->vk;

    test->ds = vk_create_image(
        vk, test->depth_format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    vk_create_image_render_view(vk, test->ds, test->aspect_mask);

    test->depth_att = (VkRenderingAttachmentInfo){
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = test->ds->render_view,
        .imageLayout = test->depth_layout,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {
            .depthStencil = {
                .depth = 0.5f,
            },
        },
    };
    test->stencil_att = (VkRenderingAttachmentInfo){
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = test->ds->render_view,
        .imageLayout = test->stencil_layout,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {
            .depthStencil = {
                .stencil = 127,
            },
        },
    };
    test->rendering_info = (VkRenderingInfo){
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .extent = {
                .width = test->width,
                .height = test->height,
            },
        },
        .layerCount = 1,
        .pDepthAttachment = &test->depth_att,
        .pStencilAttachment = &test->stencil_att,
    };
}

static void
separate_ds_test_init(struct separate_ds_test *test)
{
    struct vk *vk = &test->vk;
    vk_init(vk, NULL);

    if (!vk->vulkan_12_features.separateDepthStencilLayouts)
        vk_die("missing separateDepthStencilLayouts support");
    if (!vk->vulkan_13_features.dynamicRendering)
        vk_die("missing dynamicRendering support");

    separate_ds_test_init_image(test);
    separate_ds_test_init_pipeline(test);
    separate_ds_test_init_buffers(test);
}

static void
separate_ds_test_cleanup(struct separate_ds_test *test)
{
    struct vk *vk = &test->vk;

    if (test->d_buf)
        vk_destroy_buffer(vk, test->d_buf);
    if (test->s_buf)
        vk_destroy_buffer(vk, test->s_buf);

    vk_destroy_pipeline(vk, test->pipeline);

    vk_destroy_image(vk, test->ds);

    vk_cleanup(vk);
}

static void
separate_ds_test_draw_triangle(struct separate_ds_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    VkImageMemoryBarrier2 before_barriers[2];
    uint32_t before_barrier_count = 0;
    if (test->depth_bits) {
        before_barriers[before_barrier_count++] = (VkImageMemoryBarrier2){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = test->depth_layout,
            .image = test->ds->img,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
    }
    if (test->stencil_bits) {
        before_barriers[before_barrier_count++] = (VkImageMemoryBarrier2){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = test->stencil_layout,
            .image = test->ds->img,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
    }

    const VkDependencyInfo dep_info1 = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = before_barrier_count,
        .pImageMemoryBarriers = before_barriers,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info1);

    vk->CmdBeginRendering(cmd, &test->rendering_info);
    vk_bind_pipeline(vk, test->pipeline, cmd);
    vk->CmdDraw(cmd, 3, 1, 0, 0);
    vk->CmdEndRendering(cmd);

    VkImageMemoryBarrier2 after_barriers[2];
    uint32_t after_barrier_count = 0;
    if (test->depth_bits) {
        after_barriers[after_barrier_count++] = (VkImageMemoryBarrier2){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
            .oldLayout = test->depth_layout,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .image = test->ds->img,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
	};
    }
    if (test->stencil_bits) {
        after_barriers[after_barrier_count++] = (VkImageMemoryBarrier2){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
            .oldLayout = test->stencil_layout,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .image = test->ds->img,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
	};
    }

    const VkDependencyInfo dep_info2 = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = after_barrier_count,
        .pImageMemoryBarriers = after_barriers,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info2);

    VkBufferMemoryBarrier2 copy_barriers[2];
    uint32_t copy_barrier_count = 0;
    if (test->depth_bits) {
        const VkBufferImageCopy2 copy = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .layerCount = 1,
            },
            .imageExtent = {
                .width = test->width,
                .height = test->height,
                .depth = 1,
            },
        };
        const VkCopyImageToBufferInfo2 copy_info = {
            .sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2,
            .srcImage = test->ds->img,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstBuffer = test->d_buf->buf,
            .regionCount = 1,
            .pRegions = &copy,
        };
        vk->CmdCopyImageToBuffer2(cmd, &copy_info);

        copy_barriers[copy_barrier_count++] = (VkBufferMemoryBarrier2){
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
            .buffer = test->d_buf->buf,
            .size = VK_WHOLE_SIZE,
        };
    }
    if (test->stencil_bits) {
        const VkBufferImageCopy2 copy = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT,
                .layerCount = 1,
            },
            .imageExtent = {
                .width = test->width,
                .height = test->height,
                .depth = 1,
            },
        };
        const VkCopyImageToBufferInfo2 copy_info = {
            .sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2,
            .srcImage = test->ds->img,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstBuffer = test->s_buf->buf,
            .regionCount = 1,
            .pRegions = &copy,
        };
        vk->CmdCopyImageToBuffer2(cmd, &copy_info);

        copy_barriers[copy_barrier_count++] = (VkBufferMemoryBarrier2){
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
            .buffer = test->s_buf->buf,
            .size = VK_WHOLE_SIZE,
        };
    }

    const VkDependencyInfo dep_info3 = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = copy_barrier_count,
        .pBufferMemoryBarriers = copy_barriers,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info3);
}

static void
separate_ds_test_draw(struct separate_ds_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    separate_ds_test_draw_triangle(test, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    if (test->depth_bits == 16) {
        const uint16_t *z = test->d_buf->mem_ptr;
        vk_log("z[0][0] = %.2f (0x%04x)", (float)*z / 0xffff, *z);
    } else if (test->depth_bits == 24) {
        const uint32_t *z = test->d_buf->mem_ptr;
        vk_log("z[0][0] = %.2f (0x%06x)", (float)*z / 0xffffff, *z);
    } else if (test->depth_bits == 32) {
        const float *z = test->d_buf->mem_ptr;
        vk_log("z[0][0] = %.2f", *z);
    }

    if (test->stencil_bits == 8) {
        const uint8_t *s = test->s_buf->mem_ptr;
        vk_log("s[0][0] = %d", *s);
    }
}

int
main(void)
{
    struct separate_ds_test test = {
        .depth_format = VK_FORMAT_D24_UNORM_S8_UINT,
        .depth_layout = VK_IMAGE_LAYOUT_GENERAL,
        .stencil_layout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
        .width = 300,
        .height = 300,
    };

    switch (test.depth_format) {
    case VK_FORMAT_D16_UNORM:
        test.depth_bits = 16;
        break;
    case VK_FORMAT_X8_D24_UNORM_PACK32:
        test.depth_bits = 24;
        break;
    case VK_FORMAT_D32_SFLOAT:
        test.depth_bits = 32;
        break;
    case VK_FORMAT_S8_UINT:
        test.stencil_bits = 8;
        break;
    case VK_FORMAT_D16_UNORM_S8_UINT:
        test.depth_bits = 16;
        test.stencil_bits = 8;
        break;
    case VK_FORMAT_D24_UNORM_S8_UINT:
        test.depth_bits = 24;
        test.stencil_bits = 8;
        break;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        test.depth_bits = 32;
        test.stencil_bits = 8;
        break;
    default:
        vk_die("unknown ds format");
    }
    if (test.depth_bits)
        test.aspect_mask |= VK_IMAGE_ASPECT_DEPTH_BIT;
    if (test.stencil_bits)
        test.aspect_mask |= VK_IMAGE_ASPECT_STENCIL_BIT;

    separate_ds_test_init(&test);
    separate_ds_test_draw(&test);
    separate_ds_test_cleanup(&test);

    return 0;
}
