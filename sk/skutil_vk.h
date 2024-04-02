/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef SKUTIL_VK_H
#define SKUTIL_VK_H

#include "include/gpu/vk/GrVkBackendContext.h"
#include "include/gpu/vk/VulkanExtensions.h"
#include "skutil.h"

#include <dlfcn.h>
#include <vulkan/vulkan.h>

struct sk_vk {
    void *handle;
    PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
    PFN_vkEnumerateInstanceVersion EnumerateInstanceVersion;
    PFN_vkCreateInstance CreateInstance;

    uint32_t api_version;
    VkInstance instance;
    PFN_vkDestroyInstance DestroyInstance;
    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices;
    PFN_vkGetPhysicalDeviceFeatures2 GetPhysicalDeviceFeatures2;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties2 GetPhysicalDeviceQueueFamilyProperties2;
    PFN_vkCreateDevice CreateDevice;

    VkPhysicalDevice physical_dev;
    VkPhysicalDeviceFeatures2 features;
    VkPhysicalDeviceVulkan11Features vulkan_11_features;
    VkPhysicalDeviceVulkan12Features vulkan_12_features;
    VkPhysicalDeviceVulkan13Features vulkan_13_features;

    VkDevice dev;
    PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
    PFN_vkDestroyDevice DestroyDevice;
    PFN_vkGetDeviceQueue GetDeviceQueue;

    VkQueue queue;
    uint32_t queue_family_index;

    skgpu::VulkanExtensions exts;
    skgpu::VulkanGetProc get_proc;
};

static inline void
sk_vk_init_library(struct sk_vk *vk)
{
    const char libvulkan_name[] = "libvulkan.so.1";
    vk->handle = dlopen(libvulkan_name, RTLD_LOCAL | RTLD_LAZY);
    if (!vk->handle)
        sk_die("failed to load %s: %s", libvulkan_name, dlerror());

    const char gipa_name[] = "vkGetInstanceProcAddr";
    vk->GetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(vk->handle, gipa_name);
    if (!vk->GetInstanceProcAddr)
        sk_die("failed to find %s: %s", gipa_name, dlerror());

#define GPA(name) vk->name = (PFN_vk##name)vk->GetInstanceProcAddr(NULL, "vk" #name)
    GPA(EnumerateInstanceVersion);
    GPA(CreateInstance);
#undef GPA
}

static inline void
sk_vk_init_instance(struct sk_vk *vk)
{
    vk->api_version = VK_API_VERSION_1_3;

    const VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = vk->api_version,
    };
    const VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
    };

    VkResult result = vk->CreateInstance(&instance_info, NULL, &vk->instance);
    if (result != VK_SUCCESS)
        sk_die("failed to create instance");

#define GPA(name) vk->name = (PFN_vk##name)vk->GetInstanceProcAddr(vk->instance, "vk" #name)
    GPA(DestroyInstance);
    GPA(EnumeratePhysicalDevices);
    GPA(GetPhysicalDeviceFeatures2);
    GPA(GetPhysicalDeviceQueueFamilyProperties2);
    GPA(CreateDevice);
#undef GPA
}

static inline void
sk_vk_init_physical_device(struct sk_vk *vk)
{
    uint32_t count = 1;
    VkResult result = vk->EnumeratePhysicalDevices(vk->instance, &count, &vk->physical_dev);
    if (result < VK_SUCCESS || !count)
        sk_die("failed to enumerate physical devices");

    vk->features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    vk->features.pNext = &vk->vulkan_11_features;
    vk->vulkan_11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vk->vulkan_11_features.pNext = &vk->vulkan_12_features;
    vk->vulkan_12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk->vulkan_12_features.pNext = &vk->vulkan_13_features;
    vk->vulkan_13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vk->GetPhysicalDeviceFeatures2(vk->physical_dev, &vk->features);
}

static inline void
sk_vk_init_device(struct sk_vk *vk)
{
    VkQueueFamilyProperties2 queue_props = {
        .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2,
    };
    uint32_t queue_count = 1;
    vk->GetPhysicalDeviceQueueFamilyProperties2(vk->physical_dev, &queue_count, &queue_props);
    if (!(queue_props.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT))
        sk_die("queue family 0 does not support graphics");

    const float queue_priority = 1.0f;
    const VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };
    const VkDeviceCreateInfo dev_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &vk->features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
    };
    VkResult result = vk->CreateDevice(vk->physical_dev, &dev_info, NULL, &vk->dev);
    if (result != VK_SUCCESS)
        sk_die("failed to create device");

    vk->GetDeviceProcAddr =
        (PFN_vkGetDeviceProcAddr)vk->GetInstanceProcAddr(vk->instance, "vkGetDeviceProcAddr");

#define GPA(name) vk->name = (PFN_vk##name)vk->GetDeviceProcAddr(vk->dev, "vk" #name)
    GPA(DestroyDevice);
    GPA(GetDeviceQueue);
#undef GPA

    vk->queue_family_index = 0;
    vk->GetDeviceQueue(vk->dev, vk->queue_family_index, 0, &vk->queue);

    vk->get_proc = [vk](const char *proc_name, VkInstance instance, VkDevice device) {
        return device ? vk->GetDeviceProcAddr(device, proc_name)
                      : vk->GetInstanceProcAddr(instance, proc_name);
    };
}

static inline void
sk_vk_init(struct sk_vk *vk)
{
    *vk = {};

    sk_vk_init_library(vk);
    sk_vk_init_instance(vk);
    sk_vk_init_physical_device(vk);
    sk_vk_init_device(vk);
}

static inline void
sk_vk_cleanup(struct sk_vk *vk)
{
    vk->DestroyDevice(vk->dev, NULL);
    vk->DestroyInstance(vk->instance, NULL);

    dlclose(vk->handle);
}

static inline GrVkBackendContext
sk_vk_make_backend_context(struct sk_vk *vk)
{
    GrVkBackendContext ctx;
    ctx.fInstance = vk->instance;
    ctx.fPhysicalDevice = vk->physical_dev;
    ctx.fDevice = vk->dev;
    ctx.fQueue = vk->queue;
    ctx.fGraphicsQueueIndex = vk->queue_family_index;
    ctx.fMaxAPIVersion = vk->api_version;
    ctx.fVkExtensions = &vk->exts;
    ctx.fDeviceFeatures2 = &vk->features;
    ctx.fGetProc = vk->get_proc;

    return ctx;
}

#endif /* SKUTIL_VK_H */
