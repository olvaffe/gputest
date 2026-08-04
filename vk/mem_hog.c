/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

#include <stdatomic.h>
#include <threads.h>

static const uint32_t mem_hog_test_vs[] = {
#include "mem_hog_test.vert.inc"
};

static const uint32_t mem_hog_test_fs[] = {
#include "mem_hog_test.frag.inc"
};

struct mem_hog_test_push_const {
    uint32_t vs_loop;
    uint32_t fs_loop;
    float val;
};

struct mem_hog_test {
    struct vk vk;

    VkFormat format;
    uint32_t width;
    uint32_t height;
    struct mem_hog_test_push_const push_const;

    struct vk_image *img;
    struct vk_pipeline *pipeline;

    VkDeviceSize size;
    uint32_t count;
    struct vk_buffer **bufs;

    uint32_t sleep;

    struct {
        size_t size;
        uint32_t count;
        void **bufs;

        uint32_t sleep;

        size_t page_size;
        uint32_t page_count;
    } cpu;

    struct {
        struct vk vk;
        struct vk_buffer *buf;
    } second;

    thrd_t thread;
    atomic_bool stop;
};

static void
mem_hog_test_init_second(struct mem_hog_test *test)
{
    vk_init(&test->second.vk, NULL);
    test->second.buf =
        vk_create_buffer(&test->second.vk, 0, test->size, VK_BUFFER_USAGE_2_TRANSFER_DST_BIT);
}

static void
mem_hog_test_init_cpu(struct mem_hog_test *test)
{
    if (!test->cpu.count)
        return;

    test->cpu.bufs = calloc(test->cpu.count, sizeof(*test->cpu.bufs));
    if (!test->cpu.bufs)
        vk_die("failed to alloc sys");

    for (uint32_t i = 0; i < test->cpu.count; i++) {
        test->cpu.bufs[i] = malloc(test->cpu.size);
        if (!test->cpu.bufs[i])
            vk_die("failed to alloc sys[%d]", i);
    }

    test->cpu.page_size = sysconf(_SC_PAGESIZE);
    test->cpu.page_count = test->cpu.size / test->cpu.page_size;
}

static void
mem_hog_test_init_buffers(struct mem_hog_test *test)
{
    struct vk *vk = &test->vk;

    if (!test->count)
        return;

    test->bufs = calloc(test->count, sizeof(*test->bufs));
    if (!test->bufs)
        vk_die("failed to alloc bufs");

    for (uint32_t i = 0; i < test->count; i++)
        test->bufs[i] = vk_create_buffer(vk, 0, test->size, VK_BUFFER_USAGE_2_TRANSFER_DST_BIT);
}

static void
mem_hog_test_init_pipeline(struct mem_hog_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, mem_hog_test_vs,
                           sizeof(mem_hog_test_vs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, mem_hog_test_fs,
                           sizeof(mem_hog_test_fs));

    vk_set_pipeline_topology(vk, test->pipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

    vk_set_pipeline_viewport(vk, test->pipeline, test->width, test->height);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_FILL, false);

    vk_set_pipeline_push_const(vk, test->pipeline,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               sizeof(test->push_const));

    vk_set_pipeline_sample_count(vk, test->pipeline, VK_SAMPLE_COUNT_1_BIT);

    vk_setup_pipeline(vk, test->pipeline);
    test->pipeline->rendering_info = (VkPipelineRenderingCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &test->img->info.format,
    };
    vk_compile_pipeline(vk, test->pipeline);
}

static void
mem_hog_test_init_framebuffer(struct mem_hog_test *test)
{
    struct vk *vk = &test->vk;

    test->img =
        vk_create_image(vk, test->format, test->width, test->height, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    vk_create_image_render_view(vk, test->img, VK_IMAGE_ASPECT_COLOR_BIT);
}

static void
mem_hog_test_init(struct mem_hog_test *test)
{
    struct vk *vk = &test->vk;

    vk_init(vk, NULL);

    mem_hog_test_init_framebuffer(test);
    mem_hog_test_init_pipeline(test);

    mem_hog_test_init_buffers(test);

    mem_hog_test_init_cpu(test);
    mem_hog_test_init_second(test);
}

static void
mem_hog_test_cleanup(struct mem_hog_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_buffer(&test->second.vk, test->second.buf);
    vk_cleanup(&test->second.vk);

    for (uint32_t i = 0; i < test->cpu.count; i++)
        free(test->cpu.bufs[i]);
    free(test->cpu.bufs);

    for (uint32_t i = 0; i < test->count; i++)
        vk_destroy_buffer(vk, test->bufs[i]);
    free(test->bufs);

    vk_destroy_pipeline(vk, test->pipeline);
    vk_destroy_image(vk, test->img);

    vk_cleanup(vk);
}

static int
mem_hog_test_thread(void *arg)
{
    struct mem_hog_test *test = arg;

    while (!atomic_load(&test->stop)) {
        for (uint32_t i = 0; i < test->cpu.count; i++) {
            for (uint32_t j = 0; j < test->cpu.page_count; j++)
                memset(test->cpu.bufs[i] + test->cpu.page_size * j, 0x37, 64);
        }

        /* Force panthor to tick while
         *
         *  CSG 0
         *   - CS 0: slow vs
         *   - CS 1: sync wait for vs
         *   - CS 2: don't care
         *
         * to trigger panthor_queue_eval_syncwait.
         */
        VkCommandBuffer cmd = vk_begin_cmd(&test->second.vk, false);
        test->second.vk.CmdFillBuffer(cmd, test->second.buf->buf, 0, 64, 0x37);
        vk_end_cmd(&test->second.vk);

        if (test->cpu.sleep)
            u_sleep(test->cpu.sleep);
    }

    return 0;
}

static void
mem_hog_test_draw_buffers(struct mem_hog_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    for (uint32_t i = 0; i < test->count; i++)
        vk->CmdFillBuffer(cmd, test->bufs[i]->buf, 0, 64, 0x37);
}

static void
mem_hog_test_draw_triangle(struct mem_hog_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = test->img->img,
        .subresourceRange = subres_range,
    };
    const VkRenderingAttachmentInfo att_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = test->img->render_view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
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

    const VkDependencyInfo dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info);

    vk->CmdBeginRendering(cmd, &rendering_info);
    vk_bind_pipeline(vk, test->pipeline, cmd);
    const VkPushConstantsInfo push_info = {
        .sType = VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO,
        .layout = test->pipeline->pipeline_layout,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .size = sizeof(test->push_const),
        .pValues = &test->push_const,
    };
    vk->CmdPushConstants2(cmd, &push_info);
    vk->CmdDraw(cmd, 3, 1, 0, 0);
    vk->CmdEndRendering(cmd);
}

static void
mem_hog_test_draw(struct mem_hog_test *test)
{
    struct vk *vk = &test->vk;

    if (test->count) {
        const float gpu_mb = test->size / 1024.0f / 1024.0f;
        const float total_gpu_gb = gpu_mb * test->count / 1024.0f;
        vk_log("buf size %.1fMiB, buf count %u, total buf size %.1fGiB", gpu_mb, test->count,
               total_gpu_gb);
    }

    if (test->cpu.count) {
        const float cpu_mb = test->cpu.size / 1024.0f / 1024.0f;
        const float total_cpu_gb = cpu_mb * test->cpu.count / 1024.0f;
        vk_log("sys size %.1fMiB, sys count %u, total sys size %.1fGiB", cpu_mb, test->cpu.count,
               total_cpu_gb);
    }

    if (thrd_create(&test->thread, mem_hog_test_thread, test) != thrd_success)
        vk_die("failed to create thread");

    while (true) {
        VkCommandBuffer cmd = vk_begin_cmd(vk, false);

        mem_hog_test_draw_triangle(test, cmd);
        mem_hog_test_draw_buffers(test, cmd);

        vk_end_cmd(vk);

        if (test->sleep)
            u_sleep(test->sleep);
    }

    atomic_store(&test->stop, true);
    if (thrd_join(test->thread, NULL) != thrd_success)
        vk_die("failed to join thread");
}

int
main(int argc, char **argv)
{
    struct mem_hog_test test = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .width = 8,
        .height = 8,
        .push_const = {
            .vs_loop = 100000,
            .fs_loop = 1,
            .val = 0.0f,
        },

        .size = 1ull * 1024 * 1024,
        .count = 1024,
        .sleep = 10,
        .cpu = {
            .size = 1ull * 1024 * 1024,
            .count = 1024,
            .sleep = 5,
        },
    };

    if (argc > 1) {
        test.count = atoi(argv[1]);
        test.cpu.count = argc > 2 ? (uint32_t)atoi(argv[2]) : test.count;
    }

    mem_hog_test_init(&test);
    mem_hog_test_draw(&test);
    mem_hog_test_cleanup(&test);

    return 0;
}
