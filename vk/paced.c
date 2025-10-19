/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static const uint32_t paced_test_vs[] = {
#include "paced_test.vert.inc"
};

static const uint32_t paced_test_fs[] = {
#include "paced_test.frag.inc"
};

static const uint32_t paced_test_cs[] = {
#include "paced_test.comp.inc"
};

struct paced_push_const {
    uint32_t vs_loop;
    uint32_t fs_loop;
    uint32_t cs_loop;
    float val;
};

struct paced_test {
    VkFormat format;
    uint32_t width;
    uint32_t height;
    VkDeviceSize size;
    uint32_t interval_ms;
    uint32_t busy_ms;
    bool high_priority;

    uint32_t vertex_count;
    uint32_t group_count;
    struct paced_push_const push_const;

    struct vk vk;

    struct vk_image *img;
    struct vk_framebuffer *fb;
    struct vk_buffer *ssbo;

    struct vk_pipeline *gfx;
    struct vk_pipeline *comp;
    struct vk_descriptor_set *comp_set;
};

static void
paced_test_init_descriptor_set(struct paced_test *test)
{
    struct vk *vk = &test->vk;

    test->comp_set = vk_create_descriptor_set(vk, test->comp->set_layouts[0]);
    vk_write_descriptor_set_buffer(vk, test->comp_set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                   test->ssbo, VK_WHOLE_SIZE);
}

static void
paced_test_init_pipelines(struct paced_test *test)
{
    struct vk *vk = &test->vk;

    test->gfx = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->gfx, VK_SHADER_STAGE_VERTEX_BIT, paced_test_vs,
                           sizeof(paced_test_vs));
    vk_add_pipeline_shader(vk, test->gfx, VK_SHADER_STAGE_FRAGMENT_BIT, paced_test_fs,
                           sizeof(paced_test_fs));

    vk_set_pipeline_topology(vk, test->gfx, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

    vk_set_pipeline_viewport(vk, test->gfx, test->fb->width, test->fb->height);
    vk_set_pipeline_rasterization(vk, test->gfx, VK_POLYGON_MODE_FILL);

    vk_set_pipeline_push_const(vk, test->gfx,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               sizeof(test->push_const));

    vk_set_pipeline_sample_count(vk, test->gfx, test->fb->samples);

    vk_setup_pipeline(vk, test->gfx, test->fb);
    vk_compile_pipeline(vk, test->gfx);

    test->comp = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->comp, VK_SHADER_STAGE_COMPUTE_BIT, paced_test_cs,
                           sizeof(paced_test_cs));

    vk_add_pipeline_set_layout(vk, test->comp, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                               VK_SHADER_STAGE_COMPUTE_BIT, NULL);

    vk_set_pipeline_push_const(vk, test->comp, VK_SHADER_STAGE_COMPUTE_BIT,
                               sizeof(test->push_const));

    vk_setup_pipeline(vk, test->comp, NULL);
    vk_compile_pipeline(vk, test->comp);
}

static void
paced_test_init_ssbo(struct paced_test *test)
{
    struct vk *vk = &test->vk;

    test->ssbo = vk_create_buffer(vk, 0, test->size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

static void
paced_test_init_framebuffer(struct paced_test *test)
{
    struct vk *vk = &test->vk;

    test->img =
        vk_create_image(vk, test->format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    vk_create_image_render_view(vk, test->img, VK_IMAGE_ASPECT_COLOR_BIT);

    test->fb = vk_create_framebuffer(vk, test->img, NULL, NULL, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                     VK_ATTACHMENT_STORE_OP_STORE);
}

static void
paced_test_init(struct paced_test *test)
{
    struct vk *vk = &test->vk;

    struct vk_init_params params = {
        .high_priority = test->high_priority,
    };
    vk_init(vk, &params);

    paced_test_init_framebuffer(test);
    paced_test_init_ssbo(test);
    paced_test_init_pipelines(test);
    paced_test_init_descriptor_set(test);
}

static void
paced_test_cleanup(struct paced_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_descriptor_set(vk, test->comp_set);

    vk_destroy_pipeline(vk, test->gfx);
    vk_destroy_pipeline(vk, test->comp);

    vk_destroy_buffer(vk, test->ssbo);

    vk_destroy_image(vk, test->img);
    vk_destroy_framebuffer(vk, test->fb);

    vk_cleanup(vk);
}

static void
paced_test_draw_comp(struct paced_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkBufferMemoryBarrier pre_barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .buffer = test->ssbo->buf,
        .size = VK_WHOLE_SIZE,
    };
    const VkBufferMemoryBarrier post_barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .buffer = test->ssbo->buf,
        .size = VK_WHOLE_SIZE,
    };

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 1, &pre_barrier, 0,
                           NULL);

    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, test->comp->pipeline);
    vk->CmdPushConstants(cmd, test->comp->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(test->push_const), &test->push_const);
    vk->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, test->comp->pipeline_layout, 0,
                              1, &test->comp_set->set, 0, NULL);
    vk->CmdDispatch(cmd, test->group_count, 1, 1);

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                           0, 0, NULL, 1, &post_barrier, 0, NULL);
}

static void
paced_test_draw_gfx(struct paced_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier pre_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = test->img->img,
        .subresourceRange = subres_range,
    };
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
    };
    const VkImageMemoryBarrier post_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = test->img->img,
        .subresourceRange = subres_range,
    };

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1,
                           &pre_barrier);

    vk->CmdBeginRenderPass(cmd, &pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, test->gfx->pipeline);
    vk->CmdPushConstants(cmd, test->gfx->pipeline_layout,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(test->push_const), &test->push_const);
    vk->CmdDraw(cmd, test->vertex_count, 1, 0, 0);
    vk->CmdEndRenderPass(cmd);

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 0, NULL, 1, &post_barrier);
}

static void
paced_test_draw(struct paced_test *test, struct vk_stopwatch *stopwatch)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);
    if (stopwatch)
        vk_write_stopwatch(vk, stopwatch, cmd);

    if (test->vertex_count)
        paced_test_draw_gfx(test, cmd);
    if (test->group_count)
        paced_test_draw_comp(test, cmd);

    if (stopwatch)
        vk_write_stopwatch(vk, stopwatch, cmd);
    vk_end_cmd(vk);
}

static void
paced_test_loop(struct paced_test *test)
{
    struct vk *vk = &test->vk;

    vk_log("interval: %dms", test->interval_ms);
    vk_log("busy: %dms", test->busy_ms);
    vk_log("high priority: %d", test->high_priority);

    vk_log("calibrating...");
    struct vk_stopwatch *stopwatch = vk_create_stopwatch(vk, 2);
    uint32_t vertex_count_inc = test->vertex_count / 2;
    uint32_t group_count_inc = test->group_count / 2;
    const uint64_t calib_min = u_now() + 100ull * 1000 * 1000;
    while (true) {
        paced_test_draw(test, stopwatch);
        vk_wait(vk);

        const bool cont = u_now() < calib_min;
        const uint32_t dur_ms = vk_read_stopwatch(vk, stopwatch, 0) / 1000 / 1000;
        vk_reset_stopwatch(vk, stopwatch);
        if (dur_ms >= test->busy_ms) {
            if (cont)
                continue;
            vk_log("calibrated busy: %dms", dur_ms);
            break;
        }

        if (dur_ms * 8 < test->busy_ms) {
            vertex_count_inc *= 2;
            group_count_inc *= 2;
        }

        test->vertex_count += vertex_count_inc;
        test->group_count += group_count_inc;
    }
    vk_destroy_stopwatch(vk, stopwatch);

    vk_log("looping...");
    while (true) {
        const uint64_t begin = u_now();
        paced_test_draw(test, NULL);
        if (test->interval_ms == test->busy_ms)
            continue;

        const uint32_t dur_ms = (u_now() - begin) / 1000 / 1000;
        if (dur_ms < test->interval_ms)
            u_sleep(test->interval_ms - dur_ms);
    }
}

int
main(int argc, char **argv)
{
    struct paced_test test = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .width = 8,
        .height = 8,
        .size = sizeof(uint32_t),

        .interval_ms = 16,
        .busy_ms = 8,
        .high_priority = false,

        .vertex_count = 10 * 3,
        .group_count = 10,
        .push_const = {
            .vs_loop = 10000,
            .fs_loop = 10000,
            .cs_loop = 10000,
            .val = 0.0f,
        },
    };

    if (argc > 1)
        test.interval_ms = atoi(argv[1]);
    if (argc > 2)
        test.busy_ms = atoi(argv[2]);
    if (argc > 3)
        test.high_priority = atoi(argv[3]);

    char mesa_process_name[64];
    snprintf(mesa_process_name, sizeof(mesa_process_name), "%s-%d-%d%s", argv[0],
             test.interval_ms, test.busy_ms, test.high_priority ? "-hi" : "");
    setenv("MESA_PROCESS_NAME", mesa_process_name, true);

    paced_test_init(&test);
    paced_test_loop(&test);
    paced_test_cleanup(&test);

    return 0;
}
