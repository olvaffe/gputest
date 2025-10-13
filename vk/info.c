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

    vk_log("  features:");
    vk_log("    geometryShader: %d", vk->features.features.geometryShader);
    vk_log("    tessellationShader: %d", vk->features.features.tessellationShader);
    vk_log("    pipelineStatisticsQuery: %d", vk->features.features.pipelineStatisticsQuery);

    vk_log("  extensions:");
    for (uint32_t i = 0; i < ext_count; i++)
        vk_log("    %d: %s", i, exts[i].extensionName);

    vk_log("  %d memory heaps", vk->mem_props.memoryHeapCount);
    for (uint32_t i = 0; i < vk->mem_props.memoryHeapCount; i++) {
        const VkMemoryHeap *heap = &vk->mem_props.memoryHeaps[i];
        vk_log("    heap[%d]: size %zu flags 0x%x", i, heap->size, heap->flags);
    }

    vk_log("  %d memory types", vk->mem_props.memoryTypeCount);
    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        const VkMemoryType *mt = &vk->mem_props.memoryTypes[i];
        vk_log("    mt[%d]: heap %d flags %s%s%s%s%s%s", i, mt->heapIndex,
               (mt->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? "Lo" : "-",
               (mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? "Vi" : "-",
               (mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? "Co" : "-",
               (mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? "Ca" : "-",
               (mt->propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) ? "La" : "-",
               (mt->propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT) ? "Pr" : "-");
    }

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
    vk_init(&vk, NULL);
    info_instance(&vk);
    info_physical_device(&vk);
    vk_cleanup(&vk);

    return 0;
}
