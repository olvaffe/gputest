/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "sdlutil.h"
#include "vkutil.h"

enum win_op {
    WIN_OP_NONE,
    WIN_OP_TOGGLE_MINIMIZED,
    WIN_OP_TOGGLE_MAXIMIZED,
    WIN_OP_TOGGLE_FULLSCREEN,
};

struct sdl_test {
    uint32_t win_width;
    uint32_t win_height;
    SDL_WindowFlags win_flags;

    struct sdl sdl;
    struct vk vk;
    VkSurfaceKHR surf;

    bool quit;
    bool redraw;
    enum win_op win_op;

    struct vk_swapchain *swapchain;
};

static const char *
present_mode_str(VkPresentModeKHR mode)
{
    switch (mode) {
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
        return "VK_PRESENT_MODE_IMMEDIATE_KHR";
    case VK_PRESENT_MODE_MAILBOX_KHR:
        return "VK_PRESENT_MODE_MAILBOX_KHR";
    case VK_PRESENT_MODE_FIFO_KHR:
        return "VK_PRESENT_MODE_FIFO_KHR";
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
        return "VK_PRESENT_MODE_FIFO_RELAXED_KHR";
    case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
        return "VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR";
    case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
        return "VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR";
    case VK_PRESENT_MODE_FIFO_LATEST_READY_KHR:
        return "VK_PRESENT_MODE_FIFO_LATEST_READY_KHR";
    default:
        return "unknown";
    }
}

static void
sdl_test_dump_swapchain_caps(struct sdl_test *test)
{
    struct vk *vk = &test->vk;

    VkDeviceGroupPresentCapabilitiesKHR caps = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_CAPABILITIES_KHR,
    };
    vk->result = vk->GetDeviceGroupPresentCapabilitiesKHR(vk->dev, &caps);
    vk_check(vk, "failed to get device group present caps");

    VkDeviceGroupPresentModeFlagsKHR modes = 0;
    vk->result = vk->GetDeviceGroupSurfacePresentModesKHR(vk->dev, test->surf, &modes);
    vk_check(vk, "failed to get device group surface present modes");

    VkRect2D rects[16];
    uint32_t count = ARRAY_SIZE(rects);
    vk->result =
        vk->GetPhysicalDevicePresentRectanglesKHR(vk->physical_dev, test->surf, &count, rects);
    vk_check(vk, "failed to get present rectangles");

    vk_log("swapchain group caps:");
    vk_log("  modes: 0x%x", caps.modes);
    for (uint32_t i = 0; i < ARRAY_SIZE(caps.presentMask); i++) {
        if (caps.presentMask[i])
            vk_log("  dev %d: 0x%x", i, caps.presentMask[i]);
    }

    vk_log("swapchain group modes: 0x%x", modes);

    vk_log("swapchain rectangles:");
    for (uint32_t i = 0; i < count; i++) {
        const VkRect2D *rect = &rects[i];
        vk_log("  %d: offset (%d, %d), extent %dx%d", i, rect->offset.x, rect->offset.y,
               rect->extent.width, rect->extent.height);
    }
}

static void
sdl_test_dump_surface_caps(struct sdl_test *test, VkPresentModeKHR mode)
{
    struct vk *vk = &test->vk;

    const VkSurfacePresentModeKHR mode_info = {
        .sType = VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_KHR,
        .presentMode = mode,
    };
    const VkPhysicalDeviceSurfaceInfo2KHR info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
        .pNext = &mode_info,
        .surface = test->surf,
    };

    VkSharedPresentSurfaceCapabilitiesKHR shared_caps = {
        .sType = VK_STRUCTURE_TYPE_SHARED_PRESENT_SURFACE_CAPABILITIES_KHR,
    };
    VkPresentModeKHR compat_modes[16];
    VkSurfacePresentModeCompatibilityKHR compat_caps = {
        .sType = VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_COMPATIBILITY_KHR,
        .pNext = &shared_caps,
        .pPresentModes = compat_modes,
        .presentModeCount = ARRAY_SIZE(compat_modes),
    };
    VkSurfacePresentScalingCapabilitiesKHR scaling_caps = {
        .sType = VK_STRUCTURE_TYPE_SURFACE_PRESENT_SCALING_CAPABILITIES_KHR,
        .pNext = &compat_caps,
    };
    VkSurfaceProtectedCapabilitiesKHR prot_caps = {
        .sType = VK_STRUCTURE_TYPE_SURFACE_PROTECTED_CAPABILITIES_KHR,
        .pNext = &scaling_caps,
    };
    VkSurfaceCapabilities2KHR caps = {
        .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
        .pNext = &prot_caps,
    };

    vk->result = vk->GetPhysicalDeviceSurfaceCapabilities2KHR(vk->physical_dev, &info, &caps);
    vk_check(vk, "failed to get surface caps");

    vk_log("surface %s:", present_mode_str(mode));

    vk_log("  minImageCount=%d, maxImageCount=%d", caps.surfaceCapabilities.minImageCount,
           caps.surfaceCapabilities.maxImageCount);
    vk_log("  currentExtent=%dx%d, minExtent=%dx%d, maxExtent=%dx%d",
           caps.surfaceCapabilities.currentExtent.width,
           caps.surfaceCapabilities.currentExtent.height,
           caps.surfaceCapabilities.minImageExtent.width,
           caps.surfaceCapabilities.minImageExtent.height,
           caps.surfaceCapabilities.maxImageExtent.width,
           caps.surfaceCapabilities.maxImageExtent.height);
    vk_log("  transforms=0x%x, currentTransform=0x%x, compositeAlpha=0x%x, "
           "usages=0x%x",
           caps.surfaceCapabilities.supportedTransforms,
           caps.surfaceCapabilities.currentTransform,
           caps.surfaceCapabilities.supportedCompositeAlpha,
           caps.surfaceCapabilities.supportedUsageFlags);

    vk_log("  supportsProtected=%d", prot_caps.supportsProtected);

    vk_log("  scaling=0x%x, gravityX=0x%x, gravityY=0x%x, "
           "minExtent=%dx%d, maxExtent=%dx%d",
           scaling_caps.supportedPresentScaling, scaling_caps.supportedPresentGravityX,
           scaling_caps.supportedPresentGravityY, scaling_caps.minScaledImageExtent.width,
           scaling_caps.minScaledImageExtent.height, scaling_caps.maxScaledImageExtent.width,
           scaling_caps.maxScaledImageExtent.height);

    vk_log("  compatible modes:");
    for (uint32_t i = 0; i < compat_caps.presentModeCount; i++)
        vk_log("    %s", present_mode_str(compat_caps.pPresentModes[i]));

    vk_log("  sharedPresentSupportedUsageFlags=0x%x",
           shared_caps.sharedPresentSupportedUsageFlags);
}

static void
sdl_test_dump_surface_formats(struct sdl_test *test)
{
    struct vk *vk = &test->vk;

    const VkPhysicalDeviceSurfaceInfo2KHR info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
        .surface = test->surf,
    };

    VkSurfaceFormat2KHR fmts[32];
    uint32_t count = ARRAY_SIZE(fmts);
    for (uint32_t i = 0; i < count; i++)
        fmts[i].sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
    vk->result = vk->GetPhysicalDeviceSurfaceFormats2KHR(vk->physical_dev, &info, &count, fmts);
    vk_check(vk, "failed to get surface formats");

    vk_log("surface formats:");
    for (uint32_t i = 0; i < count; i++) {
        const VkSurfaceFormatKHR *fmt = &fmts[i].surfaceFormat;
        vk_log("  %d: format %d, colorSpace %d", i, fmt->format, fmt->colorSpace);
    }
}

static void
sdl_test_dump_surface(struct sdl_test *test)
{
    struct vk *vk = &test->vk;

    VkBool32 supported;
    vk->result = vk->GetPhysicalDeviceSurfaceSupportKHR(vk->physical_dev, vk->queue_family_index,
                                                        test->surf, &supported);
    vk_check(vk, "failed to get surface support");

    VkPresentModeKHR modes[16];
    uint32_t count = ARRAY_SIZE(modes);
    vk->result =
        vk->GetPhysicalDeviceSurfacePresentModesKHR(vk->physical_dev, test->surf, &count, modes);
    vk_check(vk, "failed to get surface present modes");

    vk_log("surface support: %d", supported);

    sdl_test_dump_surface_formats(test);

    for (uint32_t i = 0; i < count; i++)
        sdl_test_dump_surface_caps(test, modes[i]);
}

static uint32_t
sdl_test_init_instance_exts(struct sdl_test *test, const char **exts, uint32_t count)
{
    struct sdl *sdl = &test->sdl;

    const char *surface_exts[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
        VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME,
        VK_KHR_SURFACE_PROTECTED_CAPABILITIES_EXTENSION_NAME,
        VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
    };
    if (sdl->wsi_ext_count + ARRAY_SIZE(surface_exts) > count)
        vk_die("too many wsi instance exts");

    memcpy(exts, sdl->wsi_exts, sizeof(exts[0]) * sdl->wsi_ext_count);
    count = sdl->wsi_ext_count;

    for (uint32_t i = 0; i < ARRAY_SIZE(surface_exts); i++) {
        bool found = false;
        for (uint32_t j = 0; j < sdl->wsi_ext_count; j++) {
            if (!strcmp(surface_exts[i], sdl->wsi_exts[j])) {
                found = true;
                break;
            }
        }
        if (!found)
            exts[count++] = surface_exts[i];
    }

    return count;
}

static void
sdl_test_init(struct sdl_test *test)
{
    struct sdl *sdl = &test->sdl;
    struct vk *vk = &test->vk;

    const struct sdl_init_params sdl_params = {
        .libvulkan_path = LIBVULKAN_NAME,
        .width = test->win_width,
        .height = test->win_height,
        .flags = test->win_flags,
    };
    sdl_init(sdl, &sdl_params);

    const char *instance_exts[64];
    const uint32_t instance_ext_count =
        sdl_test_init_instance_exts(test, instance_exts, ARRAY_SIZE(instance_exts));

    const char *dev_exts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    const struct vk_init_params params = {
        .instance_exts = instance_exts,
        .instance_ext_count = instance_ext_count,
        .dev_exts = dev_exts,
        .dev_ext_count = ARRAY_SIZE(dev_exts),
    };
    vk_init(vk, &params);

    if (!SDL_Vulkan_CreateSurface(sdl->win, vk->instance, NULL, &test->surf))
        vk_die("failed to create surface");

    sdl_test_dump_surface(test);
    sdl_test_dump_swapchain_caps(test);
}

static void
sdl_test_cleanup(struct sdl_test *test)
{
    struct sdl *sdl = &test->sdl;
    struct vk *vk = &test->vk;

    if (test->swapchain)
        vk_destroy_swapchain(vk, test->swapchain);

    vk->DestroySurfaceKHR(vk->instance, test->surf, NULL);
    vk_cleanup(vk);

    sdl_cleanup(sdl);
}

static void
sdl_test_draw(struct sdl_test *test, struct vk_image *img)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier2 barrier1 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = img->img,
        .subresourceRange = subres_range,
    };
    const VkImageMemoryBarrier2 barrier2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_NONE,
        .dstAccessMask = VK_ACCESS_2_NONE,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = img->img,
        .subresourceRange = subres_range,
    };

    const VkDependencyInfo dep_info1 = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier1,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info1);

    const VkClearColorValue clear_val = {
        .float32 = { 1.0f, 0.5f, 0.5f, 1.0f },
    };

    vk->CmdClearColorImage(cmd, img->img, barrier1.newLayout, &clear_val, 1, &subres_range);

    const VkDependencyInfo dep_info2 = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier2,
    };
    vk->CmdPipelineBarrier2(cmd, &dep_info2);

    vk_end_cmd(vk);
    vk_wait(vk);
}

static void
sdl_test_wait_events(struct sdl_test *test)
{
    struct sdl *sdl = &test->sdl;
    SDL_Event ev;
    int32_t timeout = -1;
    while (SDL_WaitEventTimeout(&ev, timeout)) {
        timeout = 0;

        switch (ev.type) {
        case SDL_EVENT_QUIT:
            test->quit = true;
            break;
        case SDL_EVENT_WINDOW_SHOWN:
        case SDL_EVENT_WINDOW_EXPOSED:
            sdl_log_event(&ev);
            test->redraw = true;
            break;
        case SDL_EVENT_KEY_UP:
            switch (ev.key.key) {
            case SDLK_F:
                test->win_op = WIN_OP_TOGGLE_FULLSCREEN;
                break;
            case SDLK_M:
                if (ev.key.mod & SDL_KMOD_SHIFT)
                    test->win_op = WIN_OP_TOGGLE_MAXIMIZED;
                else
                    test->win_op = WIN_OP_TOGGLE_MINIMIZED;
                break;
            case SDLK_Q:
            case SDLK_ESCAPE:
                test->quit = true;
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    }
    if (timeout == -1)
        vk_die("failed to wait for events");

    /* update win size */
    int win_width;
    int win_height;
    SDL_GetWindowSizeInPixels(sdl->win, &win_width, &win_height);
    if (test->win_width != (unsigned)win_width || test->win_height != (unsigned)win_height) {
        vk_log("win resized: %dx%d -> %dx%d", test->win_width, test->win_height, win_width,
               win_height);
        test->win_width = win_width;
        test->win_height = win_height;
        test->redraw = true;
    }

    /* update win flags */
    test->win_flags = SDL_GetWindowFlags(sdl->win);

    if ((test->win_flags & SDL_WINDOW_HIDDEN) || !test->win_width || !test->win_height)
        test->redraw = false;
}

static void
sdl_test_redraw_window(struct sdl_test *test)
{
    struct vk *vk = &test->vk;

    if (!test->redraw)
        return;

    vk_log("redraw");
    test->redraw = false;

#if 0
    SDL_Surface *surf = SDL_GetWindowSurface(sdl->win);
    if (!surf)
        vk_die("no window surface");

    const SDL_PixelFormatDetails *details = SDL_GetPixelFormatDetails(surf->format);
    const uint32_t color = SDL_MapRGB(details, NULL, 0xff, 0x80, 0x80);
    SDL_FillSurfaceRect(surf, NULL, color);
    SDL_UpdateWindowSurface(sdl->win);
#else
    struct vk_image *img;

    if (!test->swapchain) {
        vk_log("create swapchain %dx%d", test->win_width, test->win_height);
        test->swapchain = vk_create_swapchain(
            vk, 0, test->surf, VK_FORMAT_B8G8R8A8_UNORM, test->win_width, test->win_height,
            VK_PRESENT_MODE_FIFO_KHR, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    }

    if (test->swapchain->info.imageExtent.width != test->win_width ||
        test->swapchain->info.imageExtent.height != test->win_height) {
        vk_log("re-create swapchain %dx%d -> %dx%d", test->swapchain->info.imageExtent.width,
               test->swapchain->info.imageExtent.height, test->win_width, test->win_height);
        vk_recreate_swapchain(vk, test->swapchain, test->win_width, test->win_height);
    }

    img = vk_acquire_swapchain_image(vk, test->swapchain);
    if (img) {
        sdl_test_draw(test, img);
        vk_present_swapchain_image(vk, test->swapchain);
    }
#endif
}

static void
sdl_test_configure_window(struct sdl_test *test)
{
    struct sdl *sdl = &test->sdl;

    switch (test->win_op) {
    case WIN_OP_TOGGLE_MINIMIZED:
        if (test->win_flags & SDL_WINDOW_MINIMIZED)
            SDL_RestoreWindow(sdl->win);
        else
            SDL_MinimizeWindow(sdl->win);
        break;
    case WIN_OP_TOGGLE_MAXIMIZED:
        if (test->win_flags & SDL_WINDOW_MAXIMIZED)
            SDL_RestoreWindow(sdl->win);
        else
            SDL_MaximizeWindow(sdl->win);
        break;
    case WIN_OP_TOGGLE_FULLSCREEN:
        SDL_SetWindowFullscreen(sdl->win, !(test->win_flags & SDL_WINDOW_FULLSCREEN));
        break;
    default:
        break;
    }

    test->win_op = WIN_OP_NONE;
}

static void
sdl_test_loop(struct sdl_test *test)
{
    while (true) {
        sdl_test_wait_events(test);

        if (test->quit)
            break;

        sdl_test_redraw_window(test);
        sdl_test_configure_window(test);
    }
}

int
main(void)
{
    struct sdl_test test = {
        .win_width = 320,
        .win_height = 240,
        .win_flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN,
    };

    sdl_test_init(&test);
    sdl_test_loop(&test);
    sdl_test_cleanup(&test);

    return 0;
}
