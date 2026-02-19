/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

#define VP_USE_OBJECT
#include <vulkan/vulkan_profiles.hpp>

struct profile_test {
    VpProfileProperties profile;
    uint32_t api_version;

    struct vk vk;
    VpCapabilities caps;
};

static void
profile_test_init(struct profile_test *test)
{
    struct vk *vk = &test->vk;

    const struct vk_init_params params = {
        .api_version = test->api_version,
    };
    vk_init(vk, &params);

    const VpVulkanFunctions funcs = {
        .GetInstanceProcAddr = vk->GetInstanceProcAddr,
        .EnumerateInstanceVersion = vk->EnumerateInstanceVersion,
        .EnumerateInstanceExtensionProperties = vk->EnumerateInstanceExtensionProperties,
        .EnumerateDeviceExtensionProperties = vk->EnumerateDeviceExtensionProperties,
        .GetPhysicalDeviceFeatures2 = vk->GetPhysicalDeviceFeatures2,
        .GetPhysicalDeviceProperties2 = vk->GetPhysicalDeviceProperties2,
        .GetPhysicalDeviceFormatProperties2 = vk->GetPhysicalDeviceFormatProperties2,
        .GetPhysicalDeviceQueueFamilyProperties2 = vk->GetPhysicalDeviceQueueFamilyProperties2,
        .CreateInstance = vk->CreateInstance,
        .CreateDevice = vk->CreateDevice,
    };
    const VpCapabilitiesCreateInfo info = {
        .apiVersion = test->api_version,
        .pVulkanFunctions = &funcs,

    };
    vpCreateCapabilities(&info, NULL, &test->caps);
}

static void
profile_test_cleanup(struct profile_test *test)
{
    struct vk *vk = &test->vk;

    vpDestroyCapabilities(test->caps, NULL);

    vk_cleanup(vk);
}

static void
profile_test_draw(struct profile_test *test)
{
    struct vk *vk = &test->vk;

    VkBool32 supported;
    vk->result = vpGetInstanceProfileSupport(test->caps, NULL, &test->profile, &supported);
    vk_check(vk, "failed to get instance profile support");

    if (!supported) {
        vk_log("%s is NOT supported at instance level", test->profile.profileName);
        return;
    }

    const VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = test->api_version,
    };
    const VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
    };
    const VpInstanceCreateInfo info = {
        .pCreateInfo = &instance_info,
        .enabledFullProfileCount = 1,
        .pEnabledFullProfiles = &test->profile,
    };

    VkInstance instance;
    vk->result = vpCreateInstance(test->caps, &info, NULL, &instance);
    vk_check(vk, "failed to create instance");

    VkPhysicalDevice physical_dev;
    uint32_t count = 1;
    vk->result = vk->EnumeratePhysicalDevices(instance, &count, &physical_dev);
    if (vk->result < VK_SUCCESS || !count)
        vk_die("failed to enumerate physical devices");

    vk->result = vpGetPhysicalDeviceProfileSupport(test->caps, instance, physical_dev,
                                                   &test->profile, &supported);
    vk_check(vk, "failed to get physical device profile support");

    vk_log("%s is %ssupported", test->profile.profileName, supported ? "" : "NOT ");

    vk->DestroyInstance(instance, NULL);
}

int
main(void)
{
    struct profile_test test = {
        .profile = {
            .profileName = VP_KHR_ROADMAP_2024_NAME,
            .specVersion = VP_KHR_ROADMAP_2024_SPEC_VERSION,
        },
        .api_version = VP_KHR_ROADMAP_2022_MIN_API_VERSION,
    };

    profile_test_init(&test);
    profile_test_draw(&test);
    profile_test_cleanup(&test);

    return 0;
}
