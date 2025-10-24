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

enum paced_test_grow {
    PACED_TEST_GROW_VERTEX = 1 << 0,
    PACED_TEST_GROW_FRAGMENT = 1 << 1,
    PACED_TEST_GROW_WORKGROUP = 1 << 2,
    PACED_TEST_GROW_VS = 1 << 3,
    PACED_TEST_GROW_FS = 1 << 4,
    PACED_TEST_GROW_CS = 1 << 5,

    PACED_TEST_GROW_DRAW = PACED_TEST_GROW_VERTEX | PACED_TEST_GROW_FRAGMENT |
                           PACED_TEST_GROW_VS | PACED_TEST_GROW_FS,
    PACED_TEST_GROW_DISPATCH = PACED_TEST_GROW_WORKGROUP | PACED_TEST_GROW_CS,
};

struct paced_test_push_const {
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
    uint32_t grow;

    bool discard;
    uint32_t vertex_count;
    uint32_t group_count;
    struct paced_test_push_const push_const;

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
    vk_set_pipeline_rasterization(vk, test->gfx, VK_POLYGON_MODE_FILL, test->discard);

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
paced_test_calibrate_grow(struct paced_test *test, uint32_t gpu_ms)
{
    uint32_t multi = 2;
    uint32_t denom = 1;
    if (gpu_ms * 8 >= test->busy_ms) {
        multi = 5;
        denom = 4;
    }

    /* we enable rasterizerDiscardEnable or use large fb when only one of them is set */
    if (test->grow & (PACED_TEST_GROW_VERTEX | PACED_TEST_GROW_FRAGMENT))
        test->vertex_count = test->vertex_count * multi / denom;

    if (test->grow & PACED_TEST_GROW_WORKGROUP)
        test->group_count = test->group_count * multi / denom;

    if (test->grow & PACED_TEST_GROW_VS)
        test->push_const.vs_loop = test->push_const.vs_loop * multi / denom;

    if (test->grow & PACED_TEST_GROW_FS)
        test->push_const.fs_loop = test->push_const.fs_loop * multi / denom;

    if (test->grow & PACED_TEST_GROW_CS)
        test->push_const.cs_loop = test->push_const.cs_loop * multi / denom;

    const uint32_t max = 1 << 28;
    if (test->vertex_count > max)
        vk_die("too many vertices");
    if (test->group_count > max)
        vk_die("too many workgroups");
    if (test->push_const.vs_loop > max)
        vk_die("too many vs loop");
    if (test->push_const.fs_loop > max)
        vk_die("too many fs loop");
    if (test->push_const.cs_loop > max)
        vk_die("too many cs loop");
}

static void
paced_test_calibrate_init(struct paced_test *test)
{
    if (test->grow & PACED_TEST_GROW_DRAW) {
        test->vertex_count = 10;
        if (test->grow & PACED_TEST_GROW_VS)
            test->push_const.vs_loop = 100;
        if (test->grow & PACED_TEST_GROW_FS)
            test->push_const.fs_loop = 100;
    }

    if (test->grow & PACED_TEST_GROW_DISPATCH) {
        test->group_count = 10;
        if (test->grow & PACED_TEST_GROW_CS)
            test->push_const.cs_loop = 100;
    }
}

static void
paced_test_calibrate(struct paced_test *test)
{
    struct vk *vk = &test->vk;
    struct vk_stopwatch *stopwatch = vk_create_stopwatch(vk, 2);

    vk_log("calibrating...");

    uint32_t dur_ms = 0;
    paced_test_calibrate_init(test);

    const uint64_t calib_min = u_now() + 100ull * 1000 * 1000;
    while (true) {
        paced_test_draw(test, stopwatch);
        vk_wait(vk);
        const bool force_cont = u_now() < calib_min;

        dur_ms = vk_read_stopwatch(vk, stopwatch, 0) / 1000 / 1000;
        vk_reset_stopwatch(vk, stopwatch);
        const bool done = dur_ms >= test->busy_ms;

        if (done || false) {
            vk_log("calibrated busy: %dms (vertex: %d, group: %d, vs: %d, fs: %d, cs: %d)",
                   dur_ms, test->vertex_count, test->group_count, test->push_const.vs_loop,
                   test->push_const.fs_loop, test->push_const.cs_loop);
        }

        if (done) {
            if (force_cont)
                continue;
            break;
        }

        paced_test_calibrate_grow(test, dur_ms);
    }

    vk_destroy_stopwatch(vk, stopwatch);
}

static void
paced_test_loop(struct paced_test *test)
{
    vk_log("interval: %dms", test->interval_ms);
    vk_log("busy: %dms", test->busy_ms);
    vk_log("high priority: %d", test->high_priority);
    vk_log("grow: 0x%x", test->grow);

    vk_log("discard: %d", test->discard);
    vk_log("size: %dx%d", test->width, test->height);

    paced_test_calibrate(test);

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
        .grow = PACED_TEST_GROW_VERTEX | PACED_TEST_GROW_FRAGMENT,
    };

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--interval"))
            test.interval_ms = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--busy"))
            test.busy_ms = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--priority"))
            test.high_priority = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--grow")) {
            const char *grow = argv[++i];

            test.grow = 0;
            if (strstr(grow, "vertex"))
                test.grow |= PACED_TEST_GROW_VERTEX;
            if (strstr(grow, "fragment"))
                test.grow |= PACED_TEST_GROW_FRAGMENT;
            if (strstr(grow, "workgroup"))
                test.grow |= PACED_TEST_GROW_WORKGROUP;
            if (strstr(grow, "vs"))
                test.grow |= PACED_TEST_GROW_VS;
            if (strstr(grow, "fs"))
                test.grow |= PACED_TEST_GROW_FS;
            if (strstr(grow, "cs"))
                test.grow |= PACED_TEST_GROW_CS;

            if (!test.grow)
                vk_die("bad grow %s", grow);
        }
    }

    if (test.grow & PACED_TEST_GROW_DRAW) {
        if (!(test.grow & (PACED_TEST_GROW_FRAGMENT | PACED_TEST_GROW_FS)))
            test.discard = true;

        if (test.grow & PACED_TEST_GROW_FRAGMENT && !(test.grow & PACED_TEST_GROW_VERTEX)) {
            test.width = 256;
            test.height = 256;
        }
    }

    char mesa_process_name[64];
    snprintf(mesa_process_name, sizeof(mesa_process_name), "%s-%d-%d%s", argv[0],
             test.interval_ms, test.busy_ms, test.high_priority ? "-hi" : "");
    setenv("MESA_PROCESS_NAME", mesa_process_name, true);

    paced_test_init(&test);
    paced_test_loop(&test);
    paced_test_cleanup(&test);

    return 0;
}
