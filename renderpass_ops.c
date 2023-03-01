/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static const uint32_t renderpass_ops_test_vs[] = {
#include "renderpass_ops_test.vert.inc"
};

static const uint32_t renderpass_ops_test_fs[] = {
#include "renderpass_ops_test.frag.inc"
};

struct renderpass_ops_test {
    bool verbose;
    VkFormat dump_color_format;
    VkFormat force_color_format;
    uint32_t width;
    uint32_t height;

    struct vk vk;

    VkCommandBuffer cmd;
    struct vk_image *color_img;
    struct vk_image *depth_img;
    struct vk_framebuffer *fb;
    struct vk_pipeline *pipeline;
};

struct renderpass_ops_test_format {
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

static struct renderpass_ops_test_format renderpass_ops_test_formats[] = {
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
renderpass_ops_test_init_formats(struct renderpass_ops_test *test)
{
    struct vk *vk = &test->vk;

    for (uint32_t i = 0; i < ARRAY_SIZE(renderpass_ops_test_formats); i++) {
        struct renderpass_ops_test_format *fmt = &renderpass_ops_test_formats[i];

        fmt->props.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
        vk->GetPhysicalDeviceFormatProperties2(vk->physical_dev, fmt->format, &fmt->props);
    }
}

static void
renderpass_ops_test_init(struct renderpass_ops_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);
    renderpass_ops_test_init_formats(test);
}

static void
renderpass_ops_test_cleanup(struct renderpass_ops_test *test)
{
    struct vk *vk = &test->vk;

    vk_cleanup(vk);
}

static VkCommandBuffer
renderpass_ops_test_begin_cmd(struct renderpass_ops_test *test)
{
    struct vk *vk = &test->vk;
    test->cmd = vk_begin_cmd(vk);
    return test->cmd;
}

static void
renderpass_ops_test_begin_framebuffer(struct renderpass_ops_test *test,
                                      const struct renderpass_ops_test_format *fmt,
                                      VkSampleCountFlagBits samples,
                                      VkImageTiling tiling,
                                      VkAttachmentLoadOp load_op,
                                      VkAttachmentStoreOp store_op)
{
    struct vk *vk = &test->vk;

    if (!test->cmd)
        vk_die("no cmd");
    if (test->color_img || test->depth_img)
        vk_die("already has img");

    /* VkImageSubresourceRange has some rules
     *
     *  - aspectMask must be only VK_IMAGE_ASPECT_COLOR_BIT,
     *    VK_IMAGE_ASPECT_DEPTH_BIT or VK_IMAGE_ASPECT_STENCIL_BIT if format
     *    is a color, depth-only or stencil-only format, respectively, except
     *    if format is a multi-planar format.
     *  - If using a depth/stencil format with both depth and stencil
     *    components, aspectMask must include at least one of
     *    VK_IMAGE_ASPECT_DEPTH_BIT and VK_IMAGE_ASPECT_STENCIL_BIT, and can
     *    include both.
     *  - When using an image view of a depth/stencil image to populate a
     *    descriptor set (e.g. for sampling in the shader, or for use as an
     *    input attachment), the aspectMask must only include one bit, which
     *    selects whether the image view is used for depth reads (i.e. using a
     *    floating-point sampler or input attachment in the shader) or stencil
     *    reads (i.e. using an unsigned integer sampler or input attachment in
     *    the shader).
     *  - When an image view of a depth/stencil image is used as a
     *    depth/stencil framebuffer attachment, the aspectMask is ignored and
     *    both depth and stencil image subresources are used.
     */
    if (fmt->color || test->force_color_format != VK_FORMAT_UNDEFINED) {
        const VkImageLayout color_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        const VkImageUsageFlags color_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        const VkImageAspectFlags color_aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        const VkAccessFlags color_access_mask =
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        test->color_img =
            vk_create_image(vk, fmt->color ? fmt->format : test->force_color_format, test->width,
                            test->height, samples, tiling, color_usage);
        vk_create_image_render_view(vk, test->color_img, color_aspect_mask);

        const VkImageMemoryBarrier color_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = color_access_mask,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = color_layout,
            .image = test->color_img->img,
            .subresourceRange = {
                .aspectMask = color_aspect_mask,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        vk->CmdPipelineBarrier(test->cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1,
                               &color_barrier);
    }

    if (fmt->depth || fmt->stencil) {
        const VkImageLayout depth_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        const VkImageUsageFlags depth_usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        const VkImageAspectFlags depth_aspect_mask =
            (fmt->depth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) |
            (fmt->stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
        const VkAccessFlags depth_access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        test->depth_img = vk_create_image(vk, fmt->format, test->width, test->height, samples,
                                          tiling, depth_usage);
        vk_create_image_render_view(vk, test->depth_img, depth_aspect_mask);

        const VkImageMemoryBarrier depth_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = depth_access_mask,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = depth_layout,
            .image = test->depth_img->img,
            .subresourceRange = {
                .aspectMask = depth_aspect_mask,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        vk->CmdPipelineBarrier(test->cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1,
                               &depth_barrier);
    }

    test->fb =
        vk_create_framebuffer(vk, test->color_img, NULL, test->depth_img, load_op, store_op);
}

static void
renderpass_ops_test_begin_pipeline(struct renderpass_ops_test *test)
{
    struct vk *vk = &test->vk;

    if (!test->fb)
        vk_die("no fb");
    if (test->pipeline)
        vk_die("already has pipeline");

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, renderpass_ops_test_vs,
                           sizeof(renderpass_ops_test_vs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT,
                           renderpass_ops_test_fs, sizeof(renderpass_ops_test_fs));

    vk_set_pipeline_topology(vk, test->pipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_FILL);

    vk_setup_pipeline(vk, test->pipeline, test->fb);
    vk_compile_pipeline(vk, test->pipeline);

    vk->CmdBindPipeline(test->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, test->pipeline->pipeline);
}

static void
renderpass_ops_test_begin_renderpass(struct renderpass_ops_test *test,
                                     const struct renderpass_ops_test_format *fmt,
                                     bool clear_att)
{
    struct vk *vk = &test->vk;

    if (!test->fb)
        vk_die("no fb");

    VkClearValue clear_vals[2];
    uint32_t clear_val_count = 0;

    if (fmt->color || test->force_color_format != VK_FORMAT_UNDEFINED)
        clear_vals[clear_val_count++].color = (VkClearColorValue){ 0 };
    if (fmt->depth || fmt->stencil) {
        clear_vals[clear_val_count++].depthStencil = (VkClearDepthStencilValue){
            .depth = 1.0f,
            .stencil = 0,
        };
    }

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
        .clearValueCount = clear_val_count,
        .pClearValues = clear_vals,
    };

    vk->CmdBeginRenderPass(test->cmd, &pass_info, VK_SUBPASS_CONTENTS_INLINE);

    if (clear_att) {
        VkClearAttachment atts[2];
        uint32_t att_count = 0;

        if (fmt->color || test->force_color_format != VK_FORMAT_UNDEFINED) {
            atts[att_count++] = (VkClearAttachment){
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .clearValue = clear_vals[0],
            };
        }

        if (fmt->depth || fmt->stencil) {
            atts[att_count++] = (VkClearAttachment){
                .aspectMask = (fmt->depth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) |
                              (fmt->stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0),
                .clearValue = clear_vals[1],
            };
        }

        const VkClearRect rect = {
            .rect = {
                .extent = {
                    .width = test->width,
                    .height = test->height,
                },
            },
            .layerCount = 1,
        };

        vk->CmdClearAttachments(test->cmd, att_count, atts, 1, &rect);
    }
}

static void
renderpass_ops_test_end_all(struct renderpass_ops_test *test, bool dump_color)
{
    struct vk *vk = &test->vk;

    if (dump_color) {
        const VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .image = test->color_img->img,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        vk->CmdPipelineBarrier(test->cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                               VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    }

    vk_end_cmd(vk);
    vk_wait(vk);

    if (dump_color)
        vk_dump_image(vk, test->color_img, VK_IMAGE_ASPECT_COLOR_BIT, "rt.ppm");

    test->cmd = NULL;

    if (test->color_img) {
        vk_destroy_image(vk, test->color_img);
        test->color_img = NULL;
    }

    if (test->depth_img) {
        vk_destroy_image(vk, test->depth_img);
        test->depth_img = NULL;
    }

    vk_destroy_framebuffer(vk, test->fb);
    test->fb = NULL;

    vk_destroy_pipeline(vk, test->pipeline);
    test->pipeline = NULL;
}

static void
renderpass_ops_test_draw_format(struct renderpass_ops_test *test,
                                const struct renderpass_ops_test_format *fmt,
                                VkImageTiling tiling)
{
    struct vk *vk = &test->vk;

    const struct {
        VkAttachmentLoadOp load_op;
        VkAttachmentStoreOp store_op;
    } combos[] = {
        {
            .load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .store_op = VK_ATTACHMENT_STORE_OP_STORE,
        },
        {
            .load_op = VK_ATTACHMENT_LOAD_OP_LOAD,
            .store_op = VK_ATTACHMENT_STORE_OP_STORE,
        },
        {
            .load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .store_op = VK_ATTACHMENT_STORE_OP_STORE,
        },
    };

    for (uint32_t i = 0; i < ARRAY_SIZE(combos); i++) {
        const VkAttachmentLoadOp load_op = combos[i].load_op;
        const VkAttachmentStoreOp store_op = combos[i].store_op;

        if (test->verbose) {
            vk_log("format %s, %s, load %d, store %d", fmt->name, tiling ? "linear" : "optimal",
                   load_op, store_op);
        }

        VkCommandBuffer cmd = renderpass_ops_test_begin_cmd(test);
        renderpass_ops_test_begin_framebuffer(test, fmt, VK_SAMPLE_COUNT_1_BIT, tiling, load_op,
                                              store_op);
        renderpass_ops_test_begin_pipeline(test);

        const bool clear_att = load_op == VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        renderpass_ops_test_begin_renderpass(test, fmt, clear_att);

        /* draw some triangles to force binning */
        for (uint32_t i = 0; i < 4; i++)
            vk->CmdDraw(cmd, 93, 1, 0, 0);

        vk->CmdEndRenderPass(cmd);

        const bool dump_color = fmt->color && fmt->format == test->dump_color_format &&
                                test->color_img->info.tiling == VK_IMAGE_TILING_LINEAR && i == 0;
        renderpass_ops_test_end_all(test, dump_color);
    }
}

static void
renderpass_ops_test_draw(struct renderpass_ops_test *test)
{
    for (uint32_t i = 0; i < ARRAY_SIZE(renderpass_ops_test_formats); i++) {
        const struct renderpass_ops_test_format *fmt = &renderpass_ops_test_formats[i];
        const VkFormatFeatureFlags linear = fmt->props.formatProperties.linearTilingFeatures;
        const VkFormatFeatureFlags optimal = fmt->props.formatProperties.optimalTilingFeatures;

        if (fmt->color) {
            if (linear & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
                renderpass_ops_test_draw_format(test, fmt, VK_IMAGE_TILING_LINEAR);
            if (optimal & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
                renderpass_ops_test_draw_format(test, fmt, VK_IMAGE_TILING_OPTIMAL);
        }

        if (fmt->depth || fmt->stencil) {
            if (linear & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
                renderpass_ops_test_draw_format(test, fmt, VK_IMAGE_TILING_LINEAR);
            if (optimal & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
                renderpass_ops_test_draw_format(test, fmt, VK_IMAGE_TILING_OPTIMAL);
        }
    }
}

int
main(void)
{
    struct renderpass_ops_test test = {
        .verbose = true,
        .dump_color_format = VK_FORMAT_B8G8R8A8_UNORM,
        .force_color_format = VK_FORMAT_B8G8R8A8_UNORM, /* to force binning */
        .width = 900,
        .height = 900,
    };

    renderpass_ops_test_init(&test);
    renderpass_ops_test_draw(&test);
    renderpass_ops_test_cleanup(&test);

    return 0;
}
