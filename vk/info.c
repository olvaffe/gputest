/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

static void
info_device_extensions(struct vk *vk)
{
    uint32_t ext_count;
    vk->EnumerateDeviceExtensionProperties(vk->physical_dev, NULL, &ext_count, NULL);

    VkExtensionProperties *exts = malloc(sizeof(*exts) * ext_count);
    if (!exts)
        vk_die("failed to alloc exts");
    vk->EnumerateDeviceExtensionProperties(vk->physical_dev, NULL, &ext_count, exts);

    vk_log("  extensions:");
    for (uint32_t i = 0; i < ext_count; i++)
        vk_log("    %d: %s", i, exts[i].extensionName);

    free(exts);
}

static void
info_device_queue_families(struct vk *vk)
{
    uint32_t qf_count;
    vk->GetPhysicalDeviceQueueFamilyProperties2(vk->physical_dev, &qf_count, NULL);

    VkQueueFamilyGlobalPriorityProperties *prio_props;
    VkQueueFamilyProperties2 *qf_props =
        calloc(qf_count, sizeof(*qf_props) + sizeof(*prio_props));
    if (!qf_props)
        vk_die("failed to alloc props");
    prio_props = (VkQueueFamilyGlobalPriorityProperties *)(qf_props + qf_count);

    for (uint32_t i = 0; i < qf_count; i++) {
        VkQueueFamilyProperties2 *qf = &qf_props[i];
        VkQueueFamilyGlobalPriorityProperties *prio = &prio_props[i];

        qf->sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
        qf->pNext = prio;

        prio->sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES;
    }
    vk->GetPhysicalDeviceQueueFamilyProperties2(vk->physical_dev, &qf_count, qf_props);

    vk_log("  queue families:");

    for (uint32_t i = 0; i < qf_count; i++) {
        const VkQueueFamilyProperties2 *qf = &qf_props[i];
        const VkQueueFamilyGlobalPriorityProperties *prio = &prio_props[i];

        enum {
            lo_bit = 1 << 0,
            md_bit = 1 << 1,
            hi_bit = 1 << 2,
            rt_bit = 1 << 3,
        };
        uint32_t prio_flags = 0;
        for (uint32_t j = 0; j < prio->priorityCount; j++) {
            switch (prio->priorities[j]) {
            case VK_QUEUE_GLOBAL_PRIORITY_LOW:
                prio_flags |= lo_bit;
                break;
            case VK_QUEUE_GLOBAL_PRIORITY_MEDIUM:
                prio_flags |= md_bit;
                break;
            case VK_QUEUE_GLOBAL_PRIORITY_HIGH:
                prio_flags |= hi_bit;
                break;
            case VK_QUEUE_GLOBAL_PRIORITY_REALTIME:
                prio_flags |= rt_bit;
                break;
            default:
                vk_die("unknown priority");
                break;
            }
        }
        vk_log("    %d: flags %s%s%s%s%s count %d ts bits %d priority %s%s%s%s", i,
               (qf->queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) ? "Gr" : "-",
               (qf->queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) ? "Co" : "-",
               (qf->queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT) ? "Tr" : "-",
               (qf->queueFamilyProperties.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) ? "Sp" : "-",
               (qf->queueFamilyProperties.queueFlags & VK_QUEUE_PROTECTED_BIT) ? "Pr" : "-",
               qf->queueFamilyProperties.queueCount, qf->queueFamilyProperties.timestampValidBits,
               (prio_flags & lo_bit) ? "Lo" : "-", (prio_flags & md_bit) ? "Md" : "-",
               (prio_flags & hi_bit) ? "Hi" : "-", (prio_flags & rt_bit) ? "Rt" : "-");
    }
}

static void
info_device_memories(struct vk *vk)
{
    vk_log("  memories:");

    for (uint32_t i = 0; i < vk->mem_props.memoryHeapCount; i++) {
        const VkMemoryHeap *heap = &vk->mem_props.memoryHeaps[i];
        vk_log("    heap %d: size %zu flags 0x%x", i, heap->size, heap->flags);
    }

    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        const VkMemoryType *mt = &vk->mem_props.memoryTypes[i];
        vk_log("    mt %d: heap %d flags %s%s%s%s%s%s", i, mt->heapIndex,
               (mt->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? "Lo" : "-",
               (mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? "Vi" : "-",
               (mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? "Co" : "-",
               (mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? "Ca" : "-",
               (mt->propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) ? "La" : "-",
               (mt->propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT) ? "Pr" : "-");
    }
}

static void
info_device_features(struct vk *vk)
{
    vk_log("  features:");

    vk_log("    geometryShader: %d", vk->features.features.geometryShader);
    vk_log("    tessellationShader: %d", vk->features.features.tessellationShader);
    vk_log("    textureCompressionETC2: %d", vk->features.features.textureCompressionETC2);
    vk_log("    textureCompressionASTC_LDR: %d",
           vk->features.features.textureCompressionASTC_LDR);
    vk_log("    textureCompressionBC: %d", vk->features.features.textureCompressionBC);
    vk_log("    pipelineStatisticsQuery: %d", vk->features.features.pipelineStatisticsQuery);

    if (vk->props.properties.apiVersion >= VK_API_VERSION_1_1) {
        vk_log("    protectedMemory: %d", vk->vulkan_11_features.protectedMemory);
        vk_log("    samplerYcbcrConversion: %d", vk->vulkan_11_features.samplerYcbcrConversion);
    } else {
        vk_log("    protectedMemory: %d", vk->protected_memory_features.protectedMemory);
        vk_log("    samplerYcbcrConversion: %d",
               vk->sampler_ycbcr_conversion_features.samplerYcbcrConversion);
    }

    if (vk->props.properties.apiVersion >= VK_API_VERSION_1_2) {
        vk_log("    descriptorIndexing: %d", vk->vulkan_12_features.descriptorIndexing);
        vk_log("    timelineSemaphore: %d", vk->vulkan_12_features.timelineSemaphore);
    }

    if (vk->props.properties.apiVersion >= VK_API_VERSION_1_3) {
        vk_log("    textureCompressionASTC_HDR: %d",
               vk->vulkan_13_features.textureCompressionASTC_HDR);
        vk_log("    dynamicRendering: %d", vk->vulkan_13_features.dynamicRendering);
        vk_log("    maintenance4: %d", vk->vulkan_13_features.maintenance4);
    }

    if (vk->props.properties.apiVersion >= VK_API_VERSION_1_4) {
        vk_log("    globalPriorityQuery: %d", vk->vulkan_14_features.globalPriorityQuery);
        vk_log("    maintenance5: %d", vk->vulkan_14_features.maintenance5);
        vk_log("    maintenance6: %d", vk->vulkan_14_features.maintenance6);
        vk_log("    pipelineProtectedAccess: %d", vk->vulkan_14_features.pipelineProtectedAccess);
        vk_log("    hostImageCopy: %d", vk->vulkan_14_features.hostImageCopy);
    } else {
        vk_log("    globalPriorityQuery: %d",
               vk->global_priority_query_features.globalPriorityQuery);
    }

    vk_log("    externalFormatResolve: %d",
           vk->external_format_resolve_features.externalFormatResolve);
}

static void
info_device_properties(struct vk *vk)
{
    vk_log("  properties:");

    vk_log("    apiVersion: %d.%d.%d", VK_API_VERSION_MAJOR(vk->props.properties.apiVersion),
           VK_API_VERSION_MINOR(vk->props.properties.apiVersion),
           VK_API_VERSION_PATCH(vk->props.properties.apiVersion));
    vk_log("    driverVersion: %d.%d.%d",
           VK_API_VERSION_MAJOR(vk->props.properties.driverVersion),
           VK_API_VERSION_MINOR(vk->props.properties.driverVersion),
           VK_API_VERSION_PATCH(vk->props.properties.driverVersion));
    vk_log("    deviceName: %s", vk->props.properties.deviceName);

    if (vk->props.properties.apiVersion >= VK_API_VERSION_1_1) {
        vk_log("    protectedNoFault: %d", vk->vulkan_11_props.protectedNoFault);
    } else {
        vk_log("    protectedNoFault: %d", vk->protected_props.protectedNoFault);
    }

    if (vk->props.properties.apiVersion >= VK_API_VERSION_1_2) {
        vk_log("    driverName: %s", vk->vulkan_12_props.driverName);
        vk_log("    driverInfo: %s", vk->vulkan_12_props.driverInfo);
    }

    vk_log("    nullColorAttachmentWithExternalFormatResolve: %d",
           vk->external_format_resolve_props.nullColorAttachmentWithExternalFormatResolve);
}

static void
info_device(struct vk *vk)
{
    uint32_t phy_count;
    vk->EnumeratePhysicalDevices(vk->instance, &phy_count, NULL);

    vk_log("device 0 (of %d):", phy_count);

    info_device_properties(vk);
    info_device_features(vk);
    info_device_memories(vk);
    info_device_queue_families(vk);
    info_device_extensions(vk);
}

static void
info_instance_extensions(struct vk *vk)
{
    uint32_t ext_count;
    vk->EnumerateInstanceExtensionProperties(NULL, &ext_count, NULL);

    VkExtensionProperties *exts = malloc(sizeof(*exts) * ext_count);
    if (!exts)
        vk_die("failed to alloc exts");
    vk->EnumerateInstanceExtensionProperties(NULL, &ext_count, exts);

    vk_log("  extensions:");
    for (uint32_t i = 0; i < ext_count; i++)
        vk_log("    %d: %s", i, exts[i].extensionName);

    free(exts);
}

static void
info_instance_version(struct vk *vk)
{
    uint32_t api_version;
    vk->EnumerateInstanceVersion(&api_version);

    vk_log("  instanceVersion: %d.%d.%d", VK_API_VERSION_MAJOR(api_version),
           VK_API_VERSION_MINOR(api_version), VK_API_VERSION_PATCH(api_version));
}

static void
info_instance(struct vk *vk)
{
    vk_log("instance:");

    info_instance_version(vk);

    vk_log("  apiVersion: %d.%d.%d", VK_API_VERSION_MAJOR(vk->params.api_version),
           VK_API_VERSION_MINOR(vk->params.api_version),
           VK_API_VERSION_PATCH(vk->params.api_version));

    info_instance_extensions(vk);
}

int
main(void)
{
    struct vk vk;

    const struct vk_init_params params = {
        .api_version = VK_API_VERSION_1_4,
        .enable_all_features = true,
    };
    vk_init(&vk, &params);

    info_instance(&vk);
    info_device(&vk);

    vk_cleanup(&vk);

    return 0;
}
