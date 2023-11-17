/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static const uint32_t depth_resolve_test_vs[] = {
#include "depth_resolve_test.vert.inc"
};

struct depth_resolve_test {
    VkFormat format;
    uint32_t format_bits;
    uint32_t width;
    uint32_t height;
    VkSampleCountFlagBits sample_count;

    struct vk vk;

    struct vk_image *ds;
    struct vk_image *resolve;

    struct vk_pipeline *pipeline;

    struct vk_buffer *buf;
};

static void
depth_resolve_test_init_buffer(struct depth_resolve_test *test)
{
    struct vk *vk = &test->vk;

    VkDeviceSize size = test->width * test->height * test->format_bits / 8;
    test->buf = vk_create_buffer(vk, 0, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
}

static void
depth_resolve_test_init_pipeline(struct depth_resolve_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, depth_resolve_test_vs,
                           sizeof(depth_resolve_test_vs));

    vk_set_pipeline_topology(vk, test->pipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

    vk_set_pipeline_viewport(vk, test->pipeline, test->width, test->height);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_FILL);

    vk_set_pipeline_sample_count(vk, test->pipeline, test->sample_count);

    vk_setup_pipeline(vk, test->pipeline, NULL);

    test->pipeline->depth_info = (VkPipelineDepthStencilStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = true,
        .depthWriteEnable = true,
        .depthCompareOp = VK_COMPARE_OP_LESS,
    };

    test->pipeline->rendering_info = (VkPipelineRenderingCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .depthAttachmentFormat = test->format,
    };

    vk_compile_pipeline(vk, test->pipeline);
}

static void
depth_resolve_test_init_images(struct depth_resolve_test *test)
{
    struct vk *vk = &test->vk;

    /* this triggers a bug on radv on gfx9 */
    const VkImageUsageFlags extra_usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    test->ds = vk_create_image(vk, test->format, test->width, test->height, test->sample_count,
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | extra_usage);
    vk_create_image_render_view(vk, test->ds, VK_IMAGE_ASPECT_DEPTH_BIT);

    test->resolve =
        vk_create_image(vk, test->format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | extra_usage);
    vk_create_image_render_view(vk, test->resolve, VK_IMAGE_ASPECT_DEPTH_BIT);
}

static void
depth_resolve_test_init(struct depth_resolve_test *test)
{
    struct vk *vk = &test->vk;

    const struct vk_init_params params = {
        .api_version = VK_API_VERSION_1_3,
        .enable_all_features = true,
    };
    vk_init(vk, &params);

    if (!vk->vulkan_13_features.dynamicRendering)
        vk_die("missing dynamicRendering support");

    depth_resolve_test_init_images(test);
    depth_resolve_test_init_pipeline(test);
    depth_resolve_test_init_buffer(test);
}

static void
depth_resolve_test_cleanup(struct depth_resolve_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_buffer(vk, test->buf);
    vk_destroy_pipeline(vk, test->pipeline);

    vk_destroy_image(vk, test->resolve);
    vk_destroy_image(vk, test->ds);

    vk_cleanup(vk);
}

static void
depth_resolve_test_draw_quad(struct depth_resolve_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageMemoryBarrier before_barriers[2] = {
        [0] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .image = test->ds->img,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        },
        [1] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .image = test->resolve->img,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        },
    };
    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           0, 0, NULL, 0, NULL, ARRAY_SIZE(before_barriers), before_barriers);

    const VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .extent = {
                .width = test->width,
                .height = test->height,
            },
        },
        .layerCount = 1,
        .pDepthAttachment = &(VkRenderingAttachmentInfo){
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = test->ds->render_view,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT,
            .resolveImageView = test->resolve->render_view,
            .resolveImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .clearValue = {
                .depthStencil = {
                    .depth = 1.0f,
                },
            },
	},
    };
    vk->CmdBeginRendering(cmd, &rendering_info);
    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, test->pipeline->pipeline);
    vk->CmdDraw(cmd, 4, 1, 0, 0);
    vk->CmdEndRendering(cmd);

    const VkImageMemoryBarrier after_barriers[1] = {
        [0] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .image = test->resolve->img,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        },
    };
    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL,
                           ARRAY_SIZE(after_barriers), after_barriers);

    const VkBufferImageCopy copy = {
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
    vk->CmdCopyImageToBuffer(cmd, test->resolve->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             test->buf->buf, 1, &copy);

    const VkBufferMemoryBarrier copy_barriers[1] = {
        [0] = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
            .buffer = test->buf->buf,
            .size = VK_WHOLE_SIZE,
        },
    };
    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                           NULL, ARRAY_SIZE(copy_barriers), copy_barriers, 0, NULL);
}

static void
depth_resolve_test_draw(struct depth_resolve_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    depth_resolve_test_draw_quad(test, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    float prev_z = 0.0f;
    for (uint32_t y = 0; y < test->height; y++) {
        const uint32_t pitch = test->width * test->format_bits / 8;
        const void *row = test->buf->mem_ptr + y * pitch;

        const uint32_t x = test->width - 1;
        const void *pixel = row + x * test->format_bits / 8;

        union {
            uint32_t u32;
            float f32;
        } val;
        float z;
        switch (test->format_bits) {
        case 16:
            val.u32 = *((const uint16_t *)pixel);
            z = (float)val.u32 / 0xffff;
            if (y == 0 || y == test->height - 1)
                vk_log("z[%d][%d] = %f (0x%04x)", x, y, z, val.u32);
            break;
        case 24:
            val.u32 = *((const uint32_t *)pixel);
            z = (float)val.u32 / 0xffffff;
            if (y == 0 || y == test->height - 1)
                vk_log("z[%d][%d] = %f (0x%06x)", x, y, z, val.u32);
            break;
        case 32:
            val.u32 = *((const uint32_t *)pixel);
            z = val.f32;
            if (y == 0 || y == test->height - 1)
                vk_log("z[%d][%d] = %f", x, y, z);
            break;
        default:
            vk_die("bad format bits");
        }

        if (z < prev_z)
            vk_die("z[%d][%d] = %f < %f", x, y, z, prev_z);
        prev_z = z;
    }
}

int
main(void)
{
    struct depth_resolve_test test = {
        .format = VK_FORMAT_D16_UNORM,
        .format_bits = 16,
        .width = 119,
        .height = 131,
        .sample_count = VK_SAMPLE_COUNT_4_BIT,
    };

    depth_resolve_test_init(&test);
    depth_resolve_test_draw(&test);
    depth_resolve_test_cleanup(&test);

    return 0;
}
