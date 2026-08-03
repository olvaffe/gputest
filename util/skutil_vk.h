/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef SKUTIL_VK_H
#define SKUTIL_VK_H

#include "include/gpu/vk/VulkanBackendContext.h"
#include "include/gpu/vk/VulkanExtensions.h"
#include "include/gpu/vk/VulkanMemoryAllocator.h"
#include "skutil.h"
#include "vkutil.h"

class gputest_vulkan_memory_allocator : public skgpu::VulkanMemoryAllocator {
  public:
    gputest_vulkan_memory_allocator(struct vk *vk) : vk(vk)
    {
        VkPhysicalDeviceMemoryProperties2 props2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
        };
        vk->GetPhysicalDeviceMemoryProperties2(vk->physical_dev, &props2);
        mem_props = props2.memoryProperties;
    }

    uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const
    {
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
            if ((type_filter & (1 << i)) &&
                (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        return UINT32_MAX;
    }

    VkResult allocateImageMemory(VkImage image,
                                 uint32_t allocationPropertyFlags,
                                 skgpu::VulkanBackendMemory *memory) override
    {
        VkMemoryRequirements reqs;
        vk->GetImageMemoryRequirements(vk->dev, image, &reqs);

        uint32_t type_idx =
            find_memory_type(reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (type_idx == UINT32_MAX)
            type_idx = find_memory_type(reqs.memoryTypeBits, 0);

        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = reqs.size;
        alloc_info.memoryTypeIndex = type_idx;

        VkDeviceMemory dev_mem = VK_NULL_HANDLE;
        VkResult res = vk->AllocateMemory(vk->dev, &alloc_info, NULL, &dev_mem);
        if (res != VK_SUCCESS)
            return res;

        res = vk->BindImageMemory(vk->dev, image, dev_mem, 0);
        if (res != VK_SUCCESS) {
            vk->FreeMemory(vk->dev, dev_mem, NULL);
            return res;
        }

        skgpu::VulkanAlloc *alloc = new skgpu::VulkanAlloc();
        alloc->fMemory = dev_mem;
        alloc->fOffset = 0;
        alloc->fSize = reqs.size;
        alloc->fFlags = 0;
        alloc->fBackendMemory = (skgpu::VulkanBackendMemory)alloc;

        *memory = (skgpu::VulkanBackendMemory)alloc;
        return VK_SUCCESS;
    }

    VkResult allocateBufferMemory(VkBuffer buffer,
                                  BufferUsage usage,
                                  uint32_t allocationPropertyFlags,
                                  skgpu::VulkanBackendMemory *memory) override
    {
        VkMemoryRequirements reqs;
        vk->GetBufferMemoryRequirements(vk->dev, buffer, &reqs);

        VkMemoryPropertyFlags props = 0;
        if (usage == BufferUsage::kCpuWritesGpuReads ||
            usage == BufferUsage::kTransfersFromCpuToGpu)
            props |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        else if (usage == BufferUsage::kTransfersFromGpuToCpu)
            props |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        else
            props |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        uint32_t type_idx = find_memory_type(reqs.memoryTypeBits, props);
        if (type_idx == UINT32_MAX)
            type_idx = find_memory_type(reqs.memoryTypeBits, 0);

        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = reqs.size;
        alloc_info.memoryTypeIndex = type_idx;

        VkDeviceMemory dev_mem = VK_NULL_HANDLE;
        VkResult res = vk->AllocateMemory(vk->dev, &alloc_info, NULL, &dev_mem);
        if (res != VK_SUCCESS)
            return res;

        res = vk->BindBufferMemory(vk->dev, buffer, dev_mem, 0);
        if (res != VK_SUCCESS) {
            vk->FreeMemory(vk->dev, dev_mem, NULL);
            return res;
        }

        skgpu::VulkanAlloc *alloc = new skgpu::VulkanAlloc();
        alloc->fMemory = dev_mem;
        alloc->fOffset = 0;
        alloc->fSize = reqs.size;
        if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            alloc->fFlags |= skgpu::VulkanAlloc::kMappable_Flag;
        if ((props & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
            alloc->fFlags |= skgpu::VulkanAlloc::kNoncoherent_Flag;
        alloc->fBackendMemory = (skgpu::VulkanBackendMemory)alloc;

        *memory = (skgpu::VulkanBackendMemory)alloc;
        return VK_SUCCESS;
    }

    void getAllocInfo(const skgpu::VulkanBackendMemory &memory,
                      skgpu::VulkanAlloc *alloc) const override
    {
        const skgpu::VulkanAlloc *a = reinterpret_cast<const skgpu::VulkanAlloc *>(memory);
        if (a && alloc)
            *alloc = *a;
    }

    VkResult mapMemory(const skgpu::VulkanBackendMemory &memory, void **data) override
    {
        const skgpu::VulkanAlloc *a = reinterpret_cast<const skgpu::VulkanAlloc *>(memory);
        if (!a)
            return VK_ERROR_INITIALIZATION_FAILED;
        const VkMemoryMapInfo map_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_MAP_INFO,
            .memory = a->fMemory,
            .size = a->fSize,
        };
        return vk->MapMemory2(vk->dev, &map_info, data);
    }

    void unmapMemory(const skgpu::VulkanBackendMemory &memory) override
    {
        const skgpu::VulkanAlloc *a = reinterpret_cast<const skgpu::VulkanAlloc *>(memory);
        if (a) {
            const VkMemoryUnmapInfo unmap_info = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_UNMAP_INFO,
                .memory = a->fMemory,
            };
            vk->UnmapMemory2(vk->dev, &unmap_info);
        }
    }

    void freeMemory(const skgpu::VulkanBackendMemory &memory) override
    {
        skgpu::VulkanAlloc *a = reinterpret_cast<skgpu::VulkanAlloc *>(memory);
        if (a) {
            vk->FreeMemory(vk->dev, a->fMemory, NULL);
            delete a;
        }
    }

    std::pair<uint64_t, uint64_t> totalAllocatedAndUsedMemory() const override
    {
        return { 0, 0 };
    }

  private:
    struct vk *vk;
    VkPhysicalDeviceMemoryProperties mem_props;
};

class sk_vk_backend_context {
  public:
    sk_vk_backend_context(struct vk *vk) : vk(vk)
    {
        get_proc = [vk](const char *proc_name, VkInstance instance, VkDevice device) {
            return device ? vk->GetDeviceProcAddr(device, proc_name)
                          : vk->GetInstanceProcAddr(instance, proc_name);
        };
    }

    skgpu::VulkanBackendContext get() const
    {
        skgpu::VulkanBackendContext ctx;
        ctx.fInstance = vk->instance;
        ctx.fPhysicalDevice = vk->physical_dev;
        ctx.fDevice = vk->dev;
        ctx.fQueue = vk->queue;
        ctx.fGraphicsQueueIndex = vk->queue_family_index;
        ctx.fMaxAPIVersion = vk->params.api_version;
        ctx.fVkExtensions = &exts;
        ctx.fDeviceFeatures2 = &vk->features;
        ctx.fGetProc = get_proc;
        ctx.fMemoryAllocator = sk_make_sp<gputest_vulkan_memory_allocator>(vk);
        return ctx;
    }

  private:
    struct vk *vk;
    skgpu::VulkanExtensions exts;
    skgpu::VulkanGetProc get_proc;
};

#endif /* SKUTIL_VK_H */
