/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

#include <android/hardware_buffer.h>
#include <binder/ProcessState.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/SurfaceControl.h>

static const uint32_t android_root_test_vs[] = {
#include "android-root_test.vert.inc"
};

static const uint32_t android_root_test_fs[] = {
#include "android-root_test.frag.inc"
};

using namespace android;

struct android_root_test {
    uint32_t width;
    uint32_t height;
    enum AHardwareBuffer_Format ahb_format;
    VkFormat vk_format;

    struct vk vk;

    sp<SurfaceComposerClient> scc;
    sp<SurfaceControl> sc;
    ANativeWindow *win;

    VkSurfaceKHR surf;

    struct vk_swapchain *swapchain;
    VkRenderingAttachmentInfo color_att;
    VkRenderingInfo rendering_info;

    struct vk_pipeline *pipeline;
};

static void
android_root_test_init_pipeline(struct android_root_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, android_root_test_vs,
                           sizeof(android_root_test_vs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, android_root_test_fs,
                           sizeof(android_root_test_fs));

    test->pipeline->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    vk_set_pipeline_viewport(vk, test->pipeline, test->width, test->height);
    test->pipeline->color_formats[test->pipeline->color_count++] = test->vk_format;

    vk_compile_pipeline(vk, test->pipeline);
}

static void
android_root_test_init_swapchain(struct android_root_test *test)
{
    struct vk *vk = &test->vk;

    test->swapchain =
        vk_create_swapchain(vk, 0, test->surf, test->vk_format, test->width, test->height,
                            VK_PRESENT_MODE_FIFO_KHR, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    for (uint32_t i = 0; i < test->swapchain->img_count; i++)
        vk_create_image_render_view(vk, &test->swapchain->imgs[i], VK_IMAGE_ASPECT_COLOR_BIT);

    test->color_att = (VkRenderingAttachmentInfo){
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = VK_NULL_HANDLE,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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
android_root_test_init_surface(struct android_root_test *test)
{
    struct vk *vk = &test->vk;

    const VkAndroidSurfaceCreateInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
        .window = test->win,
    };
    vk->result = vk->CreateAndroidSurfaceKHR(vk->instance, &info, nullptr, &test->surf);
    vk_check(vk, "failed to create surface");
}

static void
android_root_test_init_window(struct android_root_test *test)
{
    ProcessState::self()->startThreadPool();

    test->scc = sp<SurfaceComposerClient>::make();
    if (test->scc->initCheck())
        vk_die("failed to connect to SF");

    test->sc = test->scc->createSurface(String8("android-root"), test->width, test->height,
                                        test->ahb_format);
    if (!test->sc)
        vk_die("failed to create layer");

    if (SurfaceComposerClient::Transaction{}.setLayer(test->sc, INT32_MAX).show(test->sc).apply())
        vk_die("failed to config layer");

    test->win = test->sc->getSurface().get();
    if (!test->win)
        vk_die("failed to create BQ");

    if (test->win->magic != ANDROID_NATIVE_WINDOW_MAGIC)
        vk_die("unexpected ANativeWindow magic");
    if (test->win->version != sizeof(ANativeWindow))
        vk_die("unexpected ANativeWindow version");
}

static void
android_root_test_init(struct android_root_test *test)
{
    struct vk *vk = &test->vk;

    const char *instance_exts[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
    };
    const char *dev_exts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    const struct vk_init_params params = {
        .instance_exts = instance_exts,
        .instance_ext_count = ARRAY_SIZE(instance_exts),
        .dev_exts = dev_exts,
        .dev_ext_count = ARRAY_SIZE(dev_exts),
    };

    vk_init(vk, &params);

    android_root_test_init_window(test);
    android_root_test_init_surface(test);
    android_root_test_init_swapchain(test);
    android_root_test_init_pipeline(test);
}

static void
android_root_test_cleanup(struct android_root_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_pipeline(vk, test->pipeline);
    for (uint32_t i = 0; i < test->swapchain->img_count; i++)
        vk->DestroyImageView(vk->dev, test->swapchain->imgs[i].render_view, NULL);
    vk_destroy_swapchain(vk, test->swapchain);
    vk->DestroySurfaceKHR(vk->instance, test->surf, NULL);

    test->sc.clear();
    test->scc.clear();

    vk_cleanup(vk);
}

static void
android_root_test_draw_pre(struct android_root_test *test,
                           VkCommandBuffer cmd,
                           struct vk_image *img)
{
    struct vk *vk = &test->vk;

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = img->img,
        .subresourceRange = subres_range,
    };
    const VkDependencyInfo dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };

    vk->CmdPipelineBarrier2(cmd, &dep_info);
}

static void
android_root_test_draw_post(struct android_root_test *test,
                            VkCommandBuffer cmd,
                            struct vk_image *img)
{
    struct vk *vk = &test->vk;

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_NONE,
        .dstAccessMask = VK_ACCESS_2_NONE,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = img->img,
        .subresourceRange = subres_range,
    };
    const VkDependencyInfo dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };

    vk->CmdPipelineBarrier2(cmd, &dep_info);
}

static void
android_root_test_draw_triangle(struct android_root_test *test,
                                VkCommandBuffer cmd,
                                struct vk_image *img)
{
    struct vk *vk = &test->vk;

    float *red = &test->color_att.clearValue.color.float32[0];
    *red += 0.1f;
    if (*red > 1.0f)
        *red = 0.0f;
    test->color_att.imageView = img->render_view;

    vk->CmdBeginRendering(cmd, &test->rendering_info);
    vk_bind_pipeline(vk, test->pipeline, cmd);
    vk->CmdDraw(cmd, 3, 1, 0, 0);
    vk->CmdEndRendering(cmd);
}

static void
android_root_test_draw_frame(struct android_root_test *test)
{
    struct vk *vk = &test->vk;
    struct vk_image *img;

    img = vk_acquire_swapchain_image(vk, test->swapchain);

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);
    android_root_test_draw_pre(test, cmd, img);
    android_root_test_draw_triangle(test, cmd, img);
    android_root_test_draw_post(test, cmd, img);
    vk_end_cmd(vk);

    vk_present_swapchain_image(vk, test->swapchain);
}

static void
android_root_test_draw(struct android_root_test *test)
{
    const uint32_t frame_count = 100;
    const uint32_t frame_sleep = 1000 / 60;
    for (uint32_t i = 0; i < frame_count; i++) {
        android_root_test_draw_frame(test);
        u_sleep(frame_sleep);
    }
}

int
main(void)
{
    struct android_root_test test = {
        .width = 512,
        .height = 512,
        .ahb_format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
        .vk_format = VK_FORMAT_R8G8B8A8_UNORM,
    };

    android_root_test_init(&test);
    android_root_test_draw(&test);
    android_root_test_cleanup(&test);

    return 0;
}
