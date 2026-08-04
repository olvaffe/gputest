/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static const uint32_t stencil_test_vs[] = {
#include "stencil_test.vert.inc"
};

struct stencil_test {
    VkFormat depth_format;
    uint32_t width;
    uint32_t height;

    uint32_t depth_bits;
    uint32_t stencil_bits;
    VkImageAspectFlags aspect_mask;

    struct vk vk;

    struct vk_image *zs;
    VkRenderingAttachmentInfo ds_att_info;
    VkRenderingInfo rendering_info;

    struct vk_pipeline *pipeline;

    struct vk_buffer *z_buf;
    struct vk_buffer *s_buf;
};

static void
stencil_test_init_buffers(struct stencil_test *test)
{
    struct vk *vk = &test->vk;

    if (test->depth_bits) {
        VkDeviceSize size = test->width * test->height;
        size *= (test->depth_bits == 24 ? 32 : test->depth_bits) / 8;
        test->z_buf = vk_create_buffer(vk, 0, size, VK_BUFFER_USAGE_2_TRANSFER_DST_BIT);
    }

    if (test->stencil_bits) {
        const VkDeviceSize size = test->width * test->height * test->stencil_bits / 8;
        test->s_buf = vk_create_buffer(vk, 0, size, VK_BUFFER_USAGE_2_TRANSFER_DST_BIT);
    }
}

static void
stencil_test_init_pipeline(struct stencil_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, stencil_test_vs,
                           sizeof(stencil_test_vs));

    test->pipeline->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    vk_set_pipeline_viewport(vk, test->pipeline, test->width, test->height);
    /* depth test is silently skipped if depth_bits == 0 */
    test->pipeline->depth_test = true;
    test->pipeline->depth_write = true;
    test->pipeline->depth_compare_op = VK_COMPARE_OP_LESS;
    /* depth test is silently skipped if stencil_bits == 0 */
    test->pipeline->stencil_test = true;
    test->pipeline->stencil_front = (VkStencilOpState){
        .failOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP,
        .passOp = VK_STENCIL_OP_REPLACE,
        .depthFailOp = VK_STENCIL_OP_ZERO,
        .compareOp = VK_COMPARE_OP_LESS,
        .compareMask = 0xff,
        .writeMask = 0xff,
        .reference = 20,
    };
    test->pipeline->depth_att_format = test->depth_format;
    test->pipeline->stencil_att_format = test->depth_format;
    vk_compile_pipeline(vk, test->pipeline);
}

static void
stencil_test_init_rt(struct stencil_test *test)
{
    struct vk *vk = &test->vk;

    test->zs = vk_create_image(
        vk, test->depth_format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    vk_create_image_render_view(vk, test->zs, test->aspect_mask);

    test->ds_att_info = (VkRenderingAttachmentInfo){
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = test->zs->render_view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {
            .depthStencil = {
                .depth = 0.5f,
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
        .pDepthAttachment = &test->ds_att_info,
        .pStencilAttachment = &test->ds_att_info,
    };
}

static void
stencil_test_init(struct stencil_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);

    stencil_test_init_rt(test);
    stencil_test_init_buffers(test);
    stencil_test_init_pipeline(test);
}

static void
stencil_test_cleanup(struct stencil_test *test)
{
    struct vk *vk = &test->vk;

    if (test->z_buf)
        vk_destroy_buffer(vk, test->z_buf);
    if (test->s_buf)
        vk_destroy_buffer(vk, test->s_buf);

    vk_destroy_pipeline(vk, test->pipeline);

    vk_destroy_image(vk, test->zs);

    vk_cleanup(vk);
}

static void
stencil_test_draw_pre(struct stencil_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
        .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .image = test->zs->img,
        .subresourceRange = {
            .aspectMask = test->aspect_mask,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    const VkDependencyInfo dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info);
}

static void
stencil_test_draw_post(struct stencil_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageMemoryBarrier2 after_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .image = test->zs->img,
        .subresourceRange = {
            .aspectMask = test->aspect_mask,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    const VkDependencyInfo dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &after_barrier,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info);

    const VkBufferImageCopy2 copy_z = {
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
    const VkBufferImageCopy2 copy_s = {
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
    const VkBufferMemoryBarrier2 copy_barriers[2] = {
        [0] = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
            .buffer = test->z_buf ? test->z_buf->buf : VK_NULL_HANDLE,
            .size = VK_WHOLE_SIZE,
        },
        [1] = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
            .buffer = test->s_buf ? test->s_buf->buf : VK_NULL_HANDLE,
            .size = VK_WHOLE_SIZE,
        },
    };
    uint32_t copy_barrier_offset = 0;
    uint32_t copy_barrier_count = 0;
    if (test->depth_bits) {
        const VkCopyImageToBufferInfo2 copy_info = {
            .sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2,
            .srcImage = test->zs->img,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstBuffer = test->z_buf->buf,
            .regionCount = 1,
            .pRegions = &copy_z,
        };
        vk->CmdCopyImageToBuffer2(cmd, &copy_info);
        copy_barrier_count++;
    } else {
        copy_barrier_offset++;
    }
    if (test->stencil_bits) {
        const VkCopyImageToBufferInfo2 copy_info = {
            .sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2,
            .srcImage = test->zs->img,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstBuffer = test->s_buf->buf,
            .regionCount = 1,
            .pRegions = &copy_s,
        };
        vk->CmdCopyImageToBuffer2(cmd, &copy_info);
        copy_barrier_count++;
    }
    const VkDependencyInfo dep_info3 = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = copy_barrier_count,
        .pBufferMemoryBarriers = &copy_barriers[copy_barrier_offset],
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info3);
}

static void
stencil_test_draw_triangle(struct stencil_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    vk->CmdBeginRendering(cmd, &test->rendering_info);

    vk_bind_pipeline(vk, test->pipeline, cmd);

    vk->CmdDraw(cmd, 3, 1, 0, 0);

    vk->CmdEndRendering(cmd);
}

static void
stencil_test_draw(struct stencil_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    stencil_test_draw_pre(test, cmd);
    stencil_test_draw_triangle(test, cmd);
    stencil_test_draw_post(test, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    if (test->depth_bits == 16) {
        const uint16_t *z = test->z_buf->mem_ptr;
        vk_log("z[0][0] = %.2f (0x%04x)", (float)*z / 0xffff, *z);
    } else if (test->depth_bits == 24) {
        const uint32_t *z = test->z_buf->mem_ptr;
        vk_log("z[0][0] = %.2f (0x%06x)", (float)*z / 0xffffff, *z);
    } else if (test->depth_bits == 32) {
        const float *z = test->z_buf->mem_ptr;
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
    struct stencil_test test = {
        .depth_format = VK_FORMAT_D24_UNORM_S8_UINT,
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

    stencil_test_init(&test);
    stencil_test_draw(&test);
    stencil_test_cleanup(&test);

    return 0;
}
