/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static const uint32_t pipeline_stats_test_vs[] = {
#include "pipeline_stats_test.vert.inc"
};

static const uint32_t pipeline_stats_test_fs[] = {
#include "pipeline_stats_test.frag.inc"
};

struct pipeline_stats_test {
    VkFormat color_format;
    uint32_t width;
    uint32_t height;

    struct vk vk;

    struct vk_image *rt;
    struct vk_pipeline *pipeline;
    struct vk_query *query;
};

static void
pipeline_stats_test_init_pipeline(struct pipeline_stats_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, pipeline_stats_test_vs,
                           sizeof(pipeline_stats_test_vs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT,
                           pipeline_stats_test_fs, sizeof(pipeline_stats_test_fs));

    vk_set_pipeline_topology(vk, test->pipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

    vk_set_pipeline_viewport(vk, test->pipeline, test->width, test->height);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_FILL);

    vk_set_pipeline_sample_count(vk, test->pipeline, VK_SAMPLE_COUNT_1_BIT);

    vk_setup_pipeline(vk, test->pipeline, NULL);
    test->pipeline->rendering_info = (VkPipelineRenderingCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &test->color_format,
    };

    vk_compile_pipeline(vk, test->pipeline);
}

static void
pipeline_stats_test_init_framebuffer(struct pipeline_stats_test *test)
{
    struct vk *vk = &test->vk;

    test->rt =
        vk_create_image(vk, test->color_format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    vk_create_image_render_view(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT);
}

static void
pipeline_stats_test_init(struct pipeline_stats_test *test)
{
    struct vk *vk = &test->vk;
    const struct vk_init_params params = {
        .api_version = VK_API_VERSION_1_3,
        .enable_all_features = true,
    };

    vk_init(vk, &params);

    pipeline_stats_test_init_framebuffer(test);
    pipeline_stats_test_init_pipeline(test);
    test->query = vk_create_query(vk, VK_QUERY_TYPE_PIPELINE_STATISTICS, 1);
}

static void
pipeline_stats_test_cleanup(struct pipeline_stats_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_query(vk, test->query);
    vk_destroy_pipeline(vk, test->pipeline);
    vk_destroy_image(vk, test->rt);

    vk_cleanup(vk);
}

static void
pipeline_stats_test_draw_triangle(struct pipeline_stats_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    vk->ResetQueryPool(vk->dev, test->query->pool, 0, 1);

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier before_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = test->rt->img,
        .subresourceRange = subres_range,
    };
    const VkImageMemoryBarrier after_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = test->rt->img,
        .subresourceRange = subres_range,
    };

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1,
                           &before_barrier);

    const VkRenderingAttachmentInfo att_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = test->rt->render_view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {
            .color = {
                .uint32 = { 0x00, 0x10, 0x20, 0x30 },
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
    vk->CmdBeginQuery(cmd, test->query->pool, 0, 0);
    vk->CmdBeginRendering(cmd, &rendering_info);
    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, test->pipeline->pipeline);
    vk->CmdDraw(cmd, 3, 1, 0, 0);
    vk->CmdEndRendering(cmd);
    vk->CmdEndQuery(cmd, test->query->pool, 0);

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 0, NULL, 1, &after_barrier);
}

static void
pipeline_stats_test_draw(struct pipeline_stats_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    pipeline_stats_test_draw_triangle(test, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    {
        uint64_t stats[32];
        vk->result = vk->GetQueryPoolResults(vk->dev, test->query->pool, 0, 1, sizeof(stats),
                                             stats, sizeof(stats[0]), VK_QUERY_RESULT_64_BIT);
        vk_check(vk, "failed to get query pool results");

        vk_log("INPUT_ASSEMBLY_VERTICES = %" PRIu64, stats[0]);
        vk_log("INPUT_ASSEMBLY_PRIMITIVES = %" PRIu64, stats[1]);
        vk_log("VERTEX_SHADER_INVOCATIONS = %" PRIu64, stats[2]);
        vk_log("GEOMETRY_SHADER_INVOCATIONS = %" PRIu64, stats[3]);
        vk_log("GEOMETRY_SHADER_PRIMITIVES = %" PRIu64, stats[4]);
        vk_log("CLIPPING_INVOCATIONS = %" PRIu64, stats[5]);
        vk_log("CLIPPING_PRIMITIVES = %" PRIu64, stats[6]);
        vk_log("FRAGMENT_SHADER_INVOCATIONS = %" PRIu64, stats[7]);
        vk_log("TESSELLATION_CONTROL_SHADER_PATCHES = %" PRIu64, stats[8]);
        vk_log("TESSELLATION_EVALUATION_SHADER_INVOCATIONS = %" PRIu64, stats[9]);
        vk_log("COMPUTE_SHADER_INVOCATIONS = %" PRIu64, stats[10]);
    }

    // vk_dump_image(vk, test->rt, VK_IMAGE_ASPECT_COLOR_BIT, "rt.ppm");
}

int
main(void)
{
    struct pipeline_stats_test test = {
        .color_format = VK_FORMAT_R32G32B32A32_UINT,
        .width = 30,
        .height = 30,
    };

    pipeline_stats_test_init(&test);
    pipeline_stats_test_draw(&test);
    pipeline_stats_test_cleanup(&test);

    return 0;
}
