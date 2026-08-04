/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static const uint32_t protected_test_vs[] = {
#include "protected_test.vert.inc"
};

static const uint32_t protected_test_fs[] = {
#include "protected_test.frag.inc"
};

static const float protected_test_vertices[3][5] = {
    {
        -1.0f, /* x */
        -1.0f, /* y */
        1.0f,  /* r */
        0.0f,  /* g */
        0.0f,  /* b */
    },
    {
        -1.0f,
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

static const uint16_t protected_test_indices[3] = {
    0,
    1,
    2,
};

struct protected_test {
    VkFormat color_format;
    uint32_t width;
    uint32_t height;
    bool protected;

    struct vk vk;

    struct vk_buffer *vb;
    struct vk_buffer *ib;
    struct vk_buffer *staging;

    struct vk_image *rt;
    struct vk_pipeline *pipeline;
};

static void
protected_test_init_pipeline(struct protected_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, protected_test_vs,
                           sizeof(protected_test_vs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, protected_test_fs,
                           sizeof(protected_test_fs));

    const uint32_t comp_counts[2] = { 2, 3 };
    vk_set_pipeline_vertices(vk, test->pipeline, comp_counts, ARRAY_SIZE(comp_counts));
    vk_set_pipeline_topology(vk, test->pipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    vk_set_pipeline_viewport(vk, test->pipeline, test->width, test->height);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_FILL, false);

    vk_set_pipeline_sample_count(vk, test->pipeline, VK_SAMPLE_COUNT_1_BIT);

    test->pipeline->rendering_info = (VkPipelineRenderingCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &test->color_format,
    };
    vk_compile_pipeline(vk, test->pipeline);
}

static void
protected_test_init_framebuffer(struct protected_test *test)
{
    struct vk *vk = &test->vk;

    const VkImageCreateInfo img_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = test->protected ? VK_IMAGE_CREATE_PROTECTED_BIT : 0,
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
        .tiling = test->protected ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    test->rt = vk_create_image_from_info(vk, &img_info);
    vk_create_image_render_view(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT);
}

static void
protected_test_init_buffers(struct protected_test *test)
{
    struct vk *vk = &test->vk;

    uint32_t protected_mt = VK_MAX_MEMORY_TYPES;
    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        const VkMemoryType *mt = &vk->mem_props.memoryTypes[i];
        if (mt->propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT) {
            protected_mt = i;
            break;
        }
    }
    if (protected_mt == VK_MAX_MEMORY_TYPES)
        vk_die("no protected mt");

    const VkDeviceSize vb_size = sizeof(protected_test_vertices);
    test->vb = vk_create_buffer(vk, 0, vb_size, VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT);
    memcpy(test->vb->mem_ptr, protected_test_vertices, vb_size);

    const VkDeviceSize ib_size = sizeof(protected_test_indices);
    const VkBufferCreateFlags ib_flags = test->protected ? VK_BUFFER_CREATE_PROTECTED_BIT : 0;
    const VkBufferUsageFlags2 ib_usage =
        VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT;
    test->ib = vk_create_buffer_with_mt(vk, ib_flags, ib_size, ib_usage,
                                        test->protected ? protected_mt : vk->buf_mt_index);

    test->staging = vk_create_buffer(vk, 0, ib_size, VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT);
    memcpy(test->staging->mem_ptr, protected_test_indices, ib_size);
}

static void
protected_test_init(struct protected_test *test)
{
    struct vk *vk = &test->vk;

    const struct vk_init_params params = {
        .protected_memory = test->protected,
    };
    vk_init(vk, &params);

    protected_test_init_buffers(test);
    protected_test_init_framebuffer(test);
    protected_test_init_pipeline(test);
}

static void
protected_test_cleanup(struct protected_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_pipeline(vk, test->pipeline);

    vk_destroy_image(vk, test->rt);

    vk_destroy_buffer(vk, test->vb);
    vk_destroy_buffer(vk, test->ib);
    vk_destroy_buffer(vk, test->staging);

    vk_cleanup(vk);
}

static void
protected_test_draw_triangle(struct protected_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkBufferCopy2 copy = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .size = sizeof(protected_test_indices),
    };
    const VkCopyBufferInfo2 copy_info = {
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer = test->staging->buf,
        .dstBuffer = test->ib->buf,
        .regionCount = 1,
        .pRegions = &copy,
    };
    vk->CmdCopyBuffer2(cmd, &copy_info);

    const VkBufferMemoryBarrier2 buf_barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT |
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_INDEX_READ_BIT,
        .buffer = test->ib->buf,
        .size = VK_WHOLE_SIZE,
    };
    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier2 img_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT |
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = test->rt->img,
        .subresourceRange = subres_range,
    };

    const VkDependencyInfo dep_info1 = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &buf_barrier,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &img_barrier,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info1);

    const VkRenderingAttachmentInfo att_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = test->rt->render_view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {
            .color = {
                .float32 = { 0.2f, 0.2f, 0.2f, 1.0f },
            },
        },
    };
    const VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .extent = {
                .width = test->width,
                .height = test->height,
            },
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &att_info,
    };
    vk->CmdBeginRendering(cmd, &rendering_info);

    vk_bind_pipeline(vk, test->pipeline, cmd);
    vk->CmdBindVertexBuffers2(cmd, 0, 1, &test->vb->buf, &(VkDeviceSize){ 0 }, NULL, NULL);
    vk->CmdBindIndexBuffer2(cmd, test->ib->buf, 0, test->ib->info.size, VK_INDEX_TYPE_UINT16);

    vk->CmdDrawIndexed(cmd, ARRAY_SIZE(protected_test_indices), 1, 0, 0, 0);

    vk->CmdEndRendering(cmd);

    if (!test->protected) {
        const VkImageMemoryBarrier2 readback_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
            .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .image = test->rt->img,
            .subresourceRange = subres_range,
        };
        const VkDependencyInfo dep_info2 = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &readback_barrier,
        };
        vk->CmdPipelineBarrier2(cmd, &dep_info2);
    }
}

static void
protected_test_draw(struct protected_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, test->protected);
    protected_test_draw_triangle(test, cmd);
    vk_end_cmd(vk);
    vk_wait(vk);

    if (!test->protected)
        vk_dump_image(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT, "rt.ppm");
}

int
main(void)
{
    struct protected_test test = {
        .color_format = VK_FORMAT_R8G8B8A8_UNORM,
        .width = 128,
        .height = 256,
        .protected = true,
    };

    protected_test_init(&test);
    protected_test_draw(&test);
    protected_test_cleanup(&test);

    return 0;
}
