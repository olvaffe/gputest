/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

#define VP_DEBUG_MESSAGE_CALLBACK profile_test_log
#define VP_USE_OBJECT
#include <vulkan/vulkan_profiles.hpp>

struct profile_test {
    VpProfileProperties profile;
    uint32_t api_version;

    struct vk vk;
    VpCapabilities caps;
};

void
profile_test_log(const char *msg)
{
    vk_log("%s", msg);
}

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

    vk_log("%s is %ssupported at instance level", test->profile.profileName,
           supported ? "" : "NOT ");

    const VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = test->api_version,
    };
    const VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
    };

    VkInstance instance;
    if (supported) {
        const VpInstanceCreateInfo info = {
            .pCreateInfo = &instance_info,
            .enabledFullProfileCount = 1,
            .pEnabledFullProfiles = &test->profile,
        };
        vk->result = vpCreateInstance(test->caps, &info, NULL, &instance);
    } else {
        vk->result = vk->CreateInstance(&instance_info, NULL, &instance);
    }
    vk_check(vk, "failed to create instance");

    VkPhysicalDevice physical_dev;
    uint32_t count = 1;
    vk->result = vk->EnumeratePhysicalDevices(instance, &count, &physical_dev);
    if (vk->result < VK_SUCCESS || !count)
        vk_die("failed to enumerate physical devices");

    vk->result = vpGetPhysicalDeviceProfileSupport(test->caps, instance, physical_dev,
                                                   &test->profile, &supported);
    vk_check(vk, "failed to get physical device profile support");

    vk_log("%s is %ssupported at physical device level", test->profile.profileName,
           supported ? "" : "NOT ");

    vk->DestroyInstance(instance, NULL);
}

static void
profile_test_parse_args(struct profile_test *test, int argc, char **argv)
{
    const char *name = argc == 2 ? argv[1] : NULL;
    const detail::VpProfileDesc *desc = name ? detail::vpGetProfileDesc(name) : NULL;
    if (!desc) {
        if (name) {
            vk_log("unsupported profile %s", name);
            vk_log(NULL);
        }
        vk_log("usage: %s <profile>", argv[0]);
        vk_log(NULL);
        vk_log("supported profiles:");
        for (uint32_t i = 0; i < detail::profileCount; i++) {
            const detail::VpProfileDesc *desc = &detail::profiles[i];
            vk_log("  %s", desc->props.profileName);
        }

        exit(name ? -1 : 0);
    }

    test->profile = desc->props;
    test->api_version = desc->minApiVersion;
}

int
main(int argc, char **argv)
{
    struct profile_test test = {};

    profile_test_parse_args(&test, argc, argv);
    profile_test_init(&test);
    profile_test_draw(&test);
    profile_test_cleanup(&test);

    return 0;
}
