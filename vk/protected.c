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
    struct vk_framebuffer *fb;
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

    vk_set_pipeline_viewport(vk, test->pipeline, test->fb->width, test->fb->height);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_FILL, false);

    vk_set_pipeline_sample_count(vk, test->pipeline, test->fb->samples);

    vk_setup_pipeline(vk, test->pipeline, test->fb);
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

    test->fb = vk_create_framebuffer(vk, test->rt, NULL, NULL, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                     VK_ATTACHMENT_STORE_OP_STORE);
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
    test->vb = vk_create_buffer(vk, 0, vb_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    memcpy(test->vb->mem_ptr, protected_test_vertices, vb_size);

    const VkDeviceSize ib_size = sizeof(protected_test_indices);
    test->ib = vk_create_buffer_with_mt(
        vk, test->protected ? VK_BUFFER_CREATE_PROTECTED_BIT : 0, ib_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        test->protected ? protected_mt : vk->buf_mt_index);

    test->staging = vk_create_buffer(vk, 0, ib_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
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
    vk_destroy_framebuffer(vk, test->fb);

    vk_destroy_buffer(vk, test->vb);
    vk_destroy_buffer(vk, test->ib);
    vk_destroy_buffer(vk, test->staging);

    vk_cleanup(vk);
}

static void
protected_test_draw_triangle(struct protected_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkBufferCopy copy = {
        .size = sizeof(protected_test_indices),
    };
    vk->CmdCopyBuffer(cmd, test->staging->buf, test->ib->buf, 1, &copy);

    const VkBufferMemoryBarrier buf_barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_INDEX_READ_BIT,
        .buffer = test->ib->buf,
        .size = VK_WHOLE_SIZE,
    };
    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier img_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = test->rt->img,
        .subresourceRange = subres_range,
    };
    vk->CmdPipelineBarrier(
        cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
        NULL, 1, &buf_barrier, 1, &img_barrier);

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
            .color = {
                .float32 = { 0.2f, 0.2f, 0.2f, 1.0f },
            },
        },
    };
    vk->CmdBeginRenderPass(cmd, &pass_info, VK_SUBPASS_CONTENTS_INLINE);

    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, test->pipeline->pipeline);
    vk->CmdBindVertexBuffers(cmd, 0, 1, &test->vb->buf, &(VkDeviceSize){ 0 });
    vk->CmdBindIndexBuffer(cmd, test->ib->buf, 0, VK_INDEX_TYPE_UINT16);

    vk->CmdDrawIndexed(cmd, ARRAY_SIZE(protected_test_indices), 1, 0, 0, 0);

    vk->CmdEndRenderPass(cmd);

    if (!test->protected) {
        const VkImageMemoryBarrier readback_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .image = test->rt->img,
            .subresourceRange = subres_range,
        };
        vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                               VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 0, NULL, 1,
                               &readback_barrier);
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
