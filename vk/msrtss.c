/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

/* This test draws an RGB triangle to a single-sampled optimal color image using
 * VK_EXT_multisampled_render_to_single_sampled for 4x MSAA rasterization,
 * copies the result to an on-demand linear image, and dumps it to a file.
 */

#include "vkutil.h"

static const uint32_t msrtss_test_vs[] = {
#include "msrtss_test.vert.inc"
};

static const uint32_t msrtss_test_fs[] = {
#include "msrtss_test.frag.inc"
};

static const float msrtss_test_vertices[3][5] = {
    {
        -1.0f, /* x */
        -1.0f, /* y */
        1.0f,  /* r */
        0.0f,  /* g */
        0.0f,  /* b */
    },
    {
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        0.0f,
    },
    {
        1.0f,
        -1.0f,
        0.0f,
        0.0f,
        1.0f,
    },
};

struct msrtss_test {
    VkFormat color_format;
    uint32_t width;
    uint32_t height;

    struct vk vk;

    struct vk_image *rt;
    VkRenderingAttachmentInfo color_att;
    VkRenderingInfo rendering_info;
    VkMultisampledRenderToSingleSampledInfoEXT msrtss_info;

    struct vk_pipeline *pipeline;

    struct vk_buffer *vb;
};

static void
msrtss_test_init_pipeline(struct msrtss_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, msrtss_test_vs,
                           sizeof(msrtss_test_vs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, msrtss_test_fs,
                           sizeof(msrtss_test_fs));

    const uint32_t comp_counts[2] = { 2, 3 };
    vk_set_pipeline_vertices(vk, test->pipeline, comp_counts, ARRAY_SIZE(comp_counts));
    test->pipeline->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    vk_set_pipeline_viewport(vk, test->pipeline, test->width, test->height);

    test->pipeline->sample_count = VK_SAMPLE_COUNT_4_BIT;

    test->pipeline->color_formats[test->pipeline->color_count++] = test->color_format;
    vk_compile_pipeline(vk, test->pipeline);
}

static void
msrtss_test_init_rt(struct msrtss_test *test)
{
    struct vk *vk = &test->vk;

    const VkImageCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = VK_IMAGE_CREATE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_BIT_EXT,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = test->color_format,
        .extent = {
            .width = test->width,
            .height = test->height,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    test->rt = vk_create_image_from_info(vk, &info);
    vk_create_image_render_view(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT);

    test->msrtss_info = (VkMultisampledRenderToSingleSampledInfoEXT){
        .sType = VK_STRUCTURE_TYPE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_INFO_EXT,
        .multisampledRenderToSingleSampledEnable = true,
        .rasterizationSamples = VK_SAMPLE_COUNT_4_BIT,
    };

    test->color_att = (VkRenderingAttachmentInfo){
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = test->rt->render_view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT,
        .resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {
            .color = {
                .float32 = { 0.2f, 0.2f, 0.2f, 1.0f },
            },
        },
    };
    test->rendering_info = (VkRenderingInfo){
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = &test->msrtss_info,
        .renderArea = {
            .extent = {
                .width = test->width,
                .height = test->height,
            },
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &test->color_att,
    };
}

static void
msrtss_test_init_vb(struct msrtss_test *test)
{
    struct vk *vk = &test->vk;

    test->vb = vk_create_buffer(vk, 0, sizeof(msrtss_test_vertices),
                                VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT);
    memcpy(test->vb->mem_ptr, msrtss_test_vertices, sizeof(msrtss_test_vertices));
}

static bool
msrtss_test_is_optimal_format(struct msrtss_test *test)
{
    struct vk *vk = &test->vk;

    VkSubpassResolvePerformanceQueryEXT query = {
        .sType = VK_STRUCTURE_TYPE_SUBPASS_RESOLVE_PERFORMANCE_QUERY_EXT,
    };
    VkFormatProperties2 props = {
        .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
        .pNext = &query,
    };
    vk->GetPhysicalDeviceFormatProperties2(vk->physical_dev, test->color_format, &props);

    return query.optimal;
}

static void
msrtss_test_init(struct msrtss_test *test)
{
    struct vk *vk = &test->vk;

    const char *const dev_exts[] = {
        VK_EXT_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_EXTENSION_NAME,
    };
    const struct vk_init_params params = {
        .dev_exts = dev_exts,
        .dev_ext_count = ARRAY_SIZE(dev_exts),
    };

    vk_init(vk, &params);

    msrtss_test_init_rt(test);
    msrtss_test_init_pipeline(test);
    msrtss_test_init_vb(test);

    vk_log("optimal resolve performance: %d", msrtss_test_is_optimal_format(test));
}

static void
msrtss_test_cleanup(struct msrtss_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_buffer(vk, test->vb);
    vk_destroy_pipeline(vk, test->pipeline);
    vk_destroy_image(vk, test->rt);

    vk_cleanup(vk);
}

static void
msrtss_test_draw_pre(struct msrtss_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = test->rt->img,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
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
msrtss_test_draw_triangle(struct msrtss_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    vk->CmdBeginRendering(cmd, &test->rendering_info);

    vk_bind_pipeline(vk, test->pipeline, cmd);
    vk->CmdBindVertexBuffers2(cmd, 0, 1, &test->vb->buf, &(VkDeviceSize){ 0 }, NULL, NULL);

    vk->CmdDraw(cmd, 3, 1, 0, 0);

    vk->CmdEndRendering(cmd);
}

static void
msrtss_test_draw_post(struct msrtss_test *test, VkCommandBuffer cmd, struct vk_buffer *buf)
{
    struct vk *vk = &test->vk;

    const VkImageMemoryBarrier2 img_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .image = test->rt->img,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    const VkDependencyInfo dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &img_barrier,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info);

    const VkBufferImageCopy2 copy = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
        .bufferOffset = 0,
        .bufferRowLength = test->width,
        .bufferImageHeight = test->height,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
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
        .srcImage = test->rt->img,
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstBuffer = buf->buf,
        .regionCount = 1,
        .pRegions = &copy,
    };
    vk->CmdCopyImageToBuffer2(cmd, &copy_info);

    const VkBufferMemoryBarrier2 buf_barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
        .buffer = buf->buf,
        .size = VK_WHOLE_SIZE,
    };
    const VkDependencyInfo dep_info_buf = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &buf_barrier,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info_buf);
}

static void
msrtss_test_draw(struct msrtss_test *test)
{
    struct vk *vk = &test->vk;

    const VkDeviceSize buf_size = test->width * test->height * 4;
    struct vk_buffer *buf = vk_create_buffer(vk, 0, buf_size, VK_BUFFER_USAGE_2_TRANSFER_DST_BIT);

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    msrtss_test_draw_pre(test, cmd);
    msrtss_test_draw_triangle(test, cmd);
    msrtss_test_draw_post(test, cmd, buf);

    vk_end_cmd(vk);
    vk_wait(vk);

    vk_write_ppm("rt.ppm", buf->mem_ptr, test->color_format, test->width, test->height,
                 test->width * 4);
    vk_destroy_buffer(vk, buf);
}

int
main(void)
{
    struct msrtss_test test = {
        .color_format = VK_FORMAT_B8G8R8A8_UNORM,
        .width = 300,
        .height = 300,
    };

    msrtss_test_init(&test);
    msrtss_test_draw(&test);
    msrtss_test_cleanup(&test);

    return 0;
}
