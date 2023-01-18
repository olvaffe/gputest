/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static void
info_physical_device(struct vk *vk)
{
    uint32_t phy_count;
    vk->EnumeratePhysicalDevices(vk->instance, &phy_count, NULL);

    VkExtensionProperties *exts;
    uint32_t ext_count;
    vk->EnumerateDeviceExtensionProperties(vk->physical_dev, NULL, &ext_count, NULL);
    exts = malloc(sizeof(*exts) * ext_count);
    if (!exts)
        vk_die("failed to alloc exts");
    vk->EnumerateDeviceExtensionProperties(vk->physical_dev, NULL, &ext_count, exts);

    vk_log("Physical Device:");
    vk_log("  count: %d", phy_count);
    vk_log("  name: %s", vk->props.properties.deviceName);
    vk_log("  version: %d.%d.%d", VK_API_VERSION_MAJOR(vk->props.properties.apiVersion),
           VK_API_VERSION_MINOR(vk->props.properties.apiVersion),
           VK_API_VERSION_PATCH(vk->props.properties.apiVersion));
    vk_log("  extensions:");
    for (uint32_t i = 0; i < ext_count; i++)
        vk_log("    %d: %s", i, exts[i].extensionName);

    free(exts);
}

static void
info_instance(struct vk *vk)
{
    uint32_t api_version;
    vk->EnumerateInstanceVersion(&api_version);

    VkExtensionProperties *exts;
    uint32_t ext_count;
    vk->EnumerateInstanceExtensionProperties(NULL, &ext_count, NULL);
    exts = malloc(sizeof(*exts) * ext_count);
    if (!exts)
        vk_die("failed to alloc exts");
    vk->EnumerateInstanceExtensionProperties(NULL, &ext_count, exts);

    vk_log("Instance:");
    vk_log("  version: %d.%d.%d", VK_API_VERSION_MAJOR(api_version),
           VK_API_VERSION_MINOR(api_version), VK_API_VERSION_PATCH(api_version));

    vk_log("  extensions:");
    for (uint32_t i = 0; i < ext_count; i++)
        vk_log("    %d: %s", i, exts[i].extensionName);

    vk_log("  requested version: %d.%d.%d", VK_API_VERSION_MAJOR(VKUTIL_MIN_API_VERSION),
           VK_API_VERSION_MINOR(VKUTIL_MIN_API_VERSION),
           VK_API_VERSION_PATCH(VKUTIL_MIN_API_VERSION));

    free(exts);
}

int
main(void)
{
    struct vk vk;
    vk_init(&vk);
    info_instance(&vk);
    info_physical_device(&vk);
    vk_cleanup(&vk);

    return 0;
}
