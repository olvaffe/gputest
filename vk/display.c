/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

struct display_test {
    VkFormat format;
    VkPresentModeKHR present_mode;
    bool protected;

    struct vk vk;

    VkDisplayKHR display;
    VkDisplayModeKHR mode;
    uint32_t plane;
    VkDisplayPropertiesKHR display_props;
    VkDisplayModePropertiesKHR mode_props;
    VkDisplayPlanePropertiesKHR plane_props;
    VkDisplayPlaneCapabilitiesKHR plane_caps;

    VkSurfaceKHR surface;
    struct vk_swapchain *swapchain;
};

static void
display_test_init_swapchain(struct display_test *test)
{
    struct vk *vk = &test->vk;

    VkSwapchainCreateFlagsKHR flags = 0;
    if (test->protected)
        flags |= VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR;

    test->swapchain = vk_create_swapchain(vk, flags, test->surface, test->format,
                                          test->mode_props.parameters.visibleRegion.width,
                                          test->mode_props.parameters.visibleRegion.height,
                                          test->present_mode, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
}

static void
display_test_init_surface(struct display_test *test)
{
    struct vk *vk = &test->vk;

    const VkDisplaySurfaceCreateInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR,
        .displayMode = test->mode,
        .planeIndex = test->plane,
        .planeStackIndex = test->plane_props.currentStackIndex,
        .transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR,
        .imageExtent = test->mode_props.parameters.visibleRegion,
    };
    if (!(test->display_props.supportedTransforms & info.transform))
        vk_check(vk, "unsupported transform");
    if (!(test->plane_caps.supportedAlpha & info.alphaMode))
        vk_check(vk, "unsupported alpha");

    vk->result = vk->CreateDisplayPlaneSurfaceKHR(vk->instance, &info, NULL, &test->surface);
    vk_check(vk, "failed to create surface");
}

static void
display_test_init_plane(struct display_test *test)
{
    struct vk *vk = &test->vk;

    /* one plane per connector, connected or not */
    uint32_t count;
    vk->result = vk->GetPhysicalDeviceDisplayPlanePropertiesKHR(vk->physical_dev, &count, NULL);
    vk_check(vk, "failed to get plane count");
    VkDisplayPlanePropertiesKHR *props = malloc(sizeof(*props) * count);
    if (!props)
        vk_die("failed to alloc planes");
    vk->result = vk->GetPhysicalDeviceDisplayPlanePropertiesKHR(vk->physical_dev, &count, props);
    vk_check(vk, "failed to get planes");

    /* use the first supported plane */
    bool found = false;
    for (uint32_t i = 0; i < count; i++) {
        VkDisplayKHR displays[8];
        uint32_t display_count = ARRAY_SIZE(displays);
        vk->result = vk->GetDisplayPlaneSupportedDisplaysKHR(vk->physical_dev, i, &display_count,
                                                             displays);
        vk_check(vk, "failed to get supported displays");

        for (uint32_t j = 0; j < display_count; j++) {
            if (displays[j] == test->display) {
                test->plane = i;
                test->plane_props = props[i];
                found = true;
                break;
            }
        }
        if (found)
            break;
    }
    if (!found)
        vk_die("failed to find supported planes");

    free(props);

    vk->result = vk->GetDisplayPlaneCapabilitiesKHR(vk->physical_dev, test->mode, test->plane,
                                                    &test->plane_caps);
    vk_check(vk, "failed to get plane caps");
}

static void
display_test_init_mode(struct display_test *test)
{
    struct vk *vk = &test->vk;

    uint32_t count;
    vk->result = vk->GetDisplayModePropertiesKHR(vk->physical_dev, test->display, &count, NULL);
    vk_check(vk, "failed to get mode count");

    VkDisplayModePropertiesKHR *modes = malloc(sizeof(*modes) * count);
    if (!modes)
        vk_check(vk, "failed to alloc modes");

    vk->result = vk->GetDisplayModePropertiesKHR(vk->physical_dev, test->display, &count, modes);
    vk_check(vk, "failed to get modes");

    /* use the first native mode */
    for (uint32_t i = 0; i < count; i++) {
        const VkDisplayModeParametersKHR *params = &modes[i].parameters;
        if (params->visibleRegion.width == test->display_props.physicalResolution.width &&
            params->visibleRegion.height == test->display_props.physicalResolution.height) {
            test->mode = modes[i].displayMode;
            test->mode_props = modes[i];
            break;
        }
    }
    if (test->mode == VK_NULL_HANDLE)
        vk_die("failed to find native mode");

    free(modes);
}

static void
display_test_init_display(struct display_test *test)
{
    struct vk *vk = &test->vk;

    /* drmModeGetResources, drmModeGetConnector, and return connected connectors */
    uint32_t count = 1;
    vk->result =
        vk->GetPhysicalDeviceDisplayPropertiesKHR(vk->physical_dev, &count, &test->display_props);
    if ((vk->result != VK_SUCCESS && vk->result != VK_INCOMPLETE) || !count)
        vk_die("failed to get display props");

    test->display = test->display_props.display;
}

static void
display_test_dump_info(struct display_test *test)
{
    const VkDisplayPropertiesKHR *props = &test->display_props;
    vk_log("display: 0x%" PRIxPTR, (uintptr_t)test->display);
    vk_log("  displayName: %s", props->displayName);
    vk_log("  physicalDimensions %dx%d", props->physicalDimensions.width,
           props->physicalDimensions.height);
    vk_log("  physicalResolution %dx%d", props->physicalResolution.width,
           props->physicalResolution.height);
    vk_log("  supportedTransforms 0x%x", props->supportedTransforms);
    vk_log("  planeReorderPossible %d", props->planeReorderPossible);
    vk_log("  persistentContent %d", props->persistentContent);

    const VkDisplayModeParametersKHR *params = &test->mode_props.parameters;
    vk_log("mode: 0x%" PRIxPTR, (uintptr_t)test->mode);
    vk_log("  visibleRegion %dx%d", params->visibleRegion.width, params->visibleRegion.height);
    vk_log("  refreshRate %.3f", params->refreshRate / 1000.0f);

    const VkDisplayPlaneCapabilitiesKHR *caps = &test->plane_caps;
    vk_log("plane: %d", test->plane);
    vk_log("  currentDisplay: 0x%" PRIxPTR, (uintptr_t)test->plane_props.currentDisplay);
    vk_log("  currentStackIndex: %d", test->plane_props.currentStackIndex);
    vk_log("  supportedAlpha: 0x%x", caps->supportedAlpha);
    vk_log("  minSrcPosition: (%d, %d)", caps->minSrcPosition.x, caps->minSrcPosition.y);
    vk_log("  maxSrcPosition: (%d, %d)", caps->maxSrcPosition.x, caps->maxSrcPosition.y);
    vk_log("  minSrcExtent: (%d, %d)", caps->minSrcExtent.width, caps->minSrcExtent.height);
    vk_log("  maxSrcExtent: (%d, %d)", caps->maxSrcExtent.width, caps->maxSrcExtent.height);
    vk_log("  minDstPosition: (%d, %d)", caps->minDstPosition.x, caps->minDstPosition.y);
    vk_log("  maxDstPosition: (%d, %d)", caps->maxDstPosition.x, caps->maxDstPosition.y);
    vk_log("  minDstExtent: (%d, %d)", caps->minDstExtent.width, caps->minDstExtent.height);
    vk_log("  maxDstExtent: (%d, %d)", caps->maxDstExtent.width, caps->maxDstExtent.height);
}

static void
display_test_init(struct display_test *test)
{
    struct vk *vk = &test->vk;

    const char *instance_exts[] = {
        VK_KHR_DISPLAY_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME,
    };
    const char *dev_exts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    const struct vk_init_params params = {
        .protected_memory = test->protected,
        .instance_exts = instance_exts,
        .instance_ext_count = ARRAY_SIZE(instance_exts),
        .dev_exts = dev_exts,
        .dev_ext_count = ARRAY_SIZE(dev_exts),
    };
    vk_init(vk, &params);

    display_test_init_display(test);
    display_test_init_mode(test);
    display_test_init_plane(test);
    display_test_init_surface(test);
    display_test_init_swapchain(test);

    display_test_dump_info(test);
}

static void
display_test_cleanup(struct display_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_swapchain(vk, test->swapchain);
    vk->DestroySurfaceKHR(vk->instance, test->surface, NULL);
    vk_cleanup(vk);
}

static void
display_test_draw(struct display_test *test)
{
    struct vk *vk = &test->vk;

    struct vk_image *img = vk_acquire_swapchain_image(vk, test->swapchain);
    if (!img)
        vk_die("failed to acquire image");

    VkCommandBuffer cmd = vk_begin_cmd(vk, test->protected);

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier barrier1 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = img->img,
        .subresourceRange = subres_range,
    };
    const VkImageMemoryBarrier barrier2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = img->img,
        .subresourceRange = subres_range,
    };

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0, 0, NULL, 0, NULL, 1, &barrier1);

    const VkClearColorValue clear_val = {
        .float32 = { 1.0f, 0.5f, 0.5f, 1.0f },
    };

    vk->CmdClearColorImage(cmd, img->img, barrier1.newLayout, &clear_val, 1, &subres_range);

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1,
                           &barrier2);

    vk_end_cmd(vk);
    vk_wait(vk);

    vk_present_swapchain_image(vk, test->swapchain);

    u_sleep(3000);
}

int
main(void)
{
    struct display_test test = {
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .present_mode = VK_PRESENT_MODE_FIFO_KHR,
        .protected = false,
    };

    display_test_init(&test);
    display_test_draw(&test);
    display_test_cleanup(&test);

    return 0;
}
