/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef SKUTIL_VK_H
#define SKUTIL_VK_H

#include "include/gpu/vk/GrVkBackendContext.h"
#include "include/gpu/vk/VulkanExtensions.h"
#include "skutil.h"
#include "vkutil.h"

class sk_vk_backend_context {
  public:
    sk_vk_backend_context(struct vk *vk) : vk(vk)
    {
        get_proc = [vk](const char *proc_name, VkInstance instance, VkDevice device) {
            return device ? vk->GetDeviceProcAddr(device, proc_name)
                          : vk->GetInstanceProcAddr(instance, proc_name);
        };
    }

    GrVkBackendContext get() const
    {
        GrVkBackendContext ctx;
        ctx.fInstance = vk->instance;
        ctx.fPhysicalDevice = vk->physical_dev;
        ctx.fDevice = vk->dev;
        ctx.fQueue = vk->queue;
        ctx.fGraphicsQueueIndex = vk->queue_family_index;
        ctx.fMaxAPIVersion = vk->params.api_version;
        ctx.fVkExtensions = &exts;
        ctx.fDeviceFeatures2 = &vk->features;
        ctx.fGetProc = get_proc;
        return ctx;
    }

  private:
    struct vk *vk;
    skgpu::VulkanExtensions exts;
    skgpu::VulkanGetProc get_proc;
};

#endif /* SKUTIL_VK_H */
