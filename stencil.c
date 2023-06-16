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
    struct vk_framebuffer *fb;

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
        test->z_buf = vk_create_buffer(vk, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    }

    if (test->stencil_bits) {
        const VkDeviceSize size = test->width * test->height * test->stencil_bits / 8;
        test->s_buf = vk_create_buffer(vk, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    }
}

static void
stencil_test_init_pipeline(struct stencil_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, stencil_test_vs,
                           sizeof(stencil_test_vs));

    vk_set_pipeline_topology(vk, test->pipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

    vk_set_pipeline_viewport(vk, test->pipeline, test->fb->width, test->fb->height);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_FILL);

    vk_set_pipeline_sample_count(vk, test->pipeline, test->fb->samples);

    vk_setup_pipeline(vk, test->pipeline, test->fb);
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
    vk_compile_pipeline(vk, test->pipeline);
}

static void
stencil_test_init_fb(struct stencil_test *test)
{
    struct vk *vk = &test->vk;

    test->zs = vk_create_image(
        vk, test->depth_format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    vk_create_image_render_view(vk, test->zs, test->aspect_mask);

    test->fb = vk_create_framebuffer(vk, NULL, NULL, test->zs, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                     VK_ATTACHMENT_STORE_OP_STORE);
}

static void
stencil_test_init(struct stencil_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);

    stencil_test_init_fb(test);
    stencil_test_init_pipeline(test);
    stencil_test_init_buffers(test);
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
    vk_destroy_framebuffer(vk, test->fb);

    vk_cleanup(vk);
}

static void
stencil_test_draw_triangle(struct stencil_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageMemoryBarrier before_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .image = test->zs->img,
        .subresourceRange = {
            .aspectMask = test->aspect_mask,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, NULL, 0, NULL, 1,
                           &before_barrier);

    const VkRenderPassBeginInfo pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = test->fb->pass,
        .framebuffer = test->fb->fb,
        .renderArea = {
            .extent = {
                .width = test->width,
                .height = test->height,
            },
        },
        .clearValueCount = 1,
        .pClearValues = &(VkClearValue){
            .depthStencil = {
                .depth = 0.5f,
                .stencil = 127,
            },
        },
    };
    vk->CmdBeginRenderPass(cmd, &pass_info, VK_SUBPASS_CONTENTS_INLINE);

    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, test->pipeline->pipeline);

    vk->CmdDraw(cmd, 3, 1, 0, 0);

    vk->CmdEndRenderPass(cmd);

    const VkImageMemoryBarrier after_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .image = test->zs->img,
        .subresourceRange = {
            .aspectMask = test->aspect_mask,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
                           &after_barrier);

    const VkBufferImageCopy copy_z = {
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
    const VkBufferImageCopy copy_s = {
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
    const VkBufferMemoryBarrier copy_barriers[2] = {
        [0] = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
            .buffer = test->z_buf ? test->z_buf->buf : VK_NULL_HANDLE,
            .size = VK_WHOLE_SIZE,
        },
        [1] = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
            .buffer = test->s_buf ? test->s_buf->buf : VK_NULL_HANDLE,
            .size = VK_WHOLE_SIZE,
        },
    };
    uint32_t copy_barrier_offset = 0;
    uint32_t copy_barrier_count = 0;
    if (test->depth_bits) {
        vk->CmdCopyImageToBuffer(cmd, test->zs->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 test->z_buf->buf, 1, &copy_z);
        copy_barrier_count++;
    } else {
        copy_barrier_offset++;
    }
    if (test->stencil_bits) {
        vk->CmdCopyImageToBuffer(cmd, test->zs->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 test->s_buf->buf, 1, &copy_s);
        copy_barrier_count++;
    }
    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                           NULL, copy_barrier_count, &copy_barriers[copy_barrier_offset], 0,
                           NULL);
}

static void
stencil_test_draw(struct stencil_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk);

    stencil_test_draw_triangle(test, cmd);

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
