/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef VKUTIL_H
#define VKUTIL_H

#include "util.h"

#include <ctype.h>
#include <dlfcn.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <vulkan/vulkan_core.h>
/* clang-format off */
#include <vulkan/vulkan_android.h>
/* clang-format on */

#ifdef __ANDROID__
#define LIBVULKAN_NAME "libvulkan.so"
#else
#define LIBVULKAN_NAME "libvulkan.so.1"
#endif

#define VKUTIL_MIN_API_VERSION VK_API_VERSION_1_4

#define vk_check(vk, format, ...)                                                                \
    do {                                                                                         \
        if ((vk)->result > VK_SUCCESS)                                                           \
            vk_log(format __VA_OPT__(, ) __VA_ARGS__);                                           \
        else if ((vk)->result < VK_SUCCESS)                                                      \
            vk_die(format __VA_OPT__(, ) __VA_ARGS__);                                           \
    } while (0)
#define vk_die(format, ...) u_die("VK", format __VA_OPT__(, ) __VA_ARGS__)
#define vk_log(format, ...) u_log("VK", format __VA_OPT__(, ) __VA_ARGS__)

struct vk_init_params {
    const char *render_node;

    uint32_t api_version;

    bool enable_all_features;
    bool require_robustness;
    bool require_sparse;
    bool require_pipeline_stats;
    bool require_bda;
    bool require_desc_indexing;

    bool protected_memory;
    bool high_priority;

    const char *const *instance_exts;
    uint32_t instance_ext_count;

    const char *const *dev_exts;
    uint32_t dev_ext_count;
};

struct vk {
    struct vk_init_params params;
    bool KHR_swapchain;
    bool EXT_custom_border_color;
    bool EXT_physical_device_drm;

    struct {
        void *handle;
#define PFN_ALL(name) PFN_vk##name name;
#include "vkutil_entrypoints.inc"
    };

    VkResult result;

    VkInstance instance;

    VkPhysicalDevice physical_dev;

    VkPhysicalDeviceProperties2 props;
    VkPhysicalDeviceVulkan11Properties vulkan_11_props;
    VkPhysicalDeviceVulkan12Properties vulkan_12_props;
    VkPhysicalDeviceVulkan13Properties vulkan_13_props;
    VkPhysicalDeviceVulkan14Properties vulkan_14_props;

    VkPhysicalDeviceExternalFormatResolvePropertiesANDROID external_format_resolve_props;
    VkPhysicalDeviceDrmPropertiesEXT drm_props;

    VkPhysicalDeviceFeatures2 features;
    VkPhysicalDeviceVulkan11Features vulkan_11_features;
    VkPhysicalDeviceVulkan12Features vulkan_12_features;
    VkPhysicalDeviceVulkan13Features vulkan_13_features;
    VkPhysicalDeviceVulkan14Features vulkan_14_features;

    VkPhysicalDeviceCustomBorderColorFeaturesEXT custom_border_color_features;
    VkPhysicalDeviceExternalFormatResolveFeaturesANDROID external_format_resolve_features;

    VkPhysicalDeviceMemoryProperties mem_props;
    uint32_t buf_mt_index;

    VkDevice dev;
    VkQueue queue;
    uint32_t queue_family_index;

    VkDescriptorPool desc_pool;

    VkCommandPool cmd_pool;
    VkCommandPool protected_cmd_pool;
    struct {
        VkSemaphore sem;
        uint64_t sem_next;

        VkCommandBuffer cmds[4];
        uint64_t sem_vals[4];
        bool protected_submits[4];
        uint32_t count;
        uint32_t next;
    } submit;
};

struct vk_buffer {
    VkBufferCreateInfo info;
    VkBufferUsageFlags2CreateInfo usage_info;
    VkBuffer buf;

    VkDeviceMemory mem;
    VkDeviceSize mem_size;
    void *mem_ptr;
    bool is_coherent;
};

struct vk_image {
    VkImageCreateInfo info;
    VkFormatFeatureFlags2 features;
    VkImage img;

    VkDeviceMemory mem;
    VkDeviceSize mem_size;
    void *mem_ptr;
    bool is_coherent;

    VkImageView render_view;

    VkSamplerYcbcrConversion ycbcr_conv;
    uint32_t ycbcr_conv_desc_count;

    VkImageView sample_view;
    VkImageViewType sample_view_type;
    VkSampler sampler;
};

struct vk_pipeline {
    VkPipelineCreateFlags2 flags2;

    VkPipelineShaderStageCreateInfo stages[5];
    VkShaderModuleCreateInfo mods[5];
    uint32_t stage_count;

    VkDescriptorSetLayout set_layouts[4];
    uint32_t set_layout_count;
    VkPushConstantRange push_const;

    /* vertex input state */
    VkVertexInputBindingDescription vi_binding;
    VkVertexInputAttributeDescription vi_attrs[16];
    uint32_t vi_attr_count;
    VkPrimitiveTopology topology;

    /* pre-rasterization shader state */
    uint32_t patch_control_points;
    VkViewport viewport;
    VkRect2D scissor;
    VkPolygonMode poly_mode;
    bool rasterizer_discard;

    /* fragment shader state */
    VkSampleCountFlagBits sample_count;
    VkPipelineDepthStencilStateCreateInfo depth_info;

    /* fragment output state */
    VkFormat color_att_format;
    VkFormat depth_att_format;
    VkFormat stencil_att_format;
    uint64_t external_format;

    VkPipelineLayout layout;
    VkPipeline pipeline;
};

struct vk_descriptor_set {
    VkDescriptorSet set;
};

struct vk_semaphore {
    VkSemaphore sem;
    VkSemaphoreType type;
    VkExternalSemaphoreHandleTypeFlagBits handle_type;
};

struct vk_event {
    VkEvent event;
};

struct vk_query {
    VkQueryPool pool;
};

struct vk_stopwatch {
    struct vk_query *query;
    uint32_t query_max;
    uint32_t query_count;

    uint64_t *ts;
};

struct vk_swapchain {
    VkSwapchainCreateInfoKHR info;
    VkSwapchainKHR swapchain;
    VkFence fence;

    uint32_t img_count;
    VkImage *img_handles;
    struct vk_image *imgs;

    uint32_t img_cur;
};

static inline void
vk_init_params(struct vk *vk, const struct vk_init_params *params)
{
    if (params)
        vk->params = *params;
    if (vk->params.api_version < VKUTIL_MIN_API_VERSION)
        vk->params.api_version = VKUTIL_MIN_API_VERSION;

    for (uint32_t i = 0; i < vk->params.dev_ext_count; i++) {
        if (!strcmp(vk->params.dev_exts[i], VK_KHR_SWAPCHAIN_EXTENSION_NAME))
            vk->KHR_swapchain = true;
        else if (!strcmp(vk->params.dev_exts[i], VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME))
            vk->EXT_custom_border_color = true;
        else if (!strcmp(vk->params.dev_exts[i], VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME))
            vk->EXT_physical_device_drm = true;
    }
}

static inline void
vk_init_global_dispatch(struct vk *vk)
{
#define PFN_GLOBAL(name) vk->name = (PFN_vk##name)vk->GetInstanceProcAddr(NULL, "vk" #name);
#include "vkutil_entrypoints.inc"
}

static inline void
vk_init_library(struct vk *vk)
{
    vk->handle = dlopen(LIBVULKAN_NAME, RTLD_LOCAL | RTLD_LAZY);
    if (!vk->handle)
        vk_die("failed to load %s: %s", LIBVULKAN_NAME, dlerror());

    const char gipa_name[] = "vkGetInstanceProcAddr";
    vk->GetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(vk->handle, gipa_name);
    if (!vk->GetInstanceProcAddr)
        vk_die("failed to find %s: %s", gipa_name, dlerror());

    vk_init_global_dispatch(vk);
}

static inline void
vk_init_instance_dispatch(struct vk *vk)
{
#define PFN_INSTANCE(name)                                                                       \
    vk->name = (PFN_vk##name)vk->GetInstanceProcAddr(vk->instance, "vk" #name);
#include "vkutil_entrypoints.inc"
}

static inline void
vk_init_instance(struct vk *vk)
{
    uint32_t api_version;
    vk->EnumerateInstanceVersion(&api_version);
    if (api_version < vk->params.api_version)
        vk_die("instance api version %d < %d", api_version, vk->params.api_version);

    const VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = vk->params.api_version,
    };
    const VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = vk->params.instance_ext_count,
        .ppEnabledExtensionNames = vk->params.instance_exts,
    };

    vk->result = vk->CreateInstance(&instance_info, NULL, &vk->instance);
    vk_check(vk, "failed to create instance: %d (no icd?)", vk->result);

    vk_init_instance_dispatch(vk);
}

static inline void
vk_init_physical_device_memory_properties(struct vk *vk)
{
    VkPhysicalDeviceMemoryProperties2 props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
    };
    vk->GetPhysicalDeviceMemoryProperties2(vk->physical_dev, &props2);
    vk->mem_props = props2.memoryProperties;

    const VkMemoryPropertyFlags mt_flags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    bool mt_found = false;
    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        const VkMemoryType *mt = &vk->mem_props.memoryTypes[i];
        if ((mt->propertyFlags & mt_flags) == mt_flags) {
            vk->buf_mt_index = i;
            mt_found = true;
            /* prefer cached */
            if (mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
                break;
        }
    }
    if (!mt_found)
        vk_die("failed to find a coherent and visible memory type for buffers");
}

static inline void
vk_init_physical_device_feature_fixups(struct vk *vk)
{
    /* mask potentially expensive features by default */

    if (vk->params.require_robustness) {
        if (!vk->features.features.robustBufferAccess ||
            !vk->vulkan_13_features.robustImageAccess ||
            !vk->vulkan_14_features.pipelineRobustness)
            vk_die("no robustness");
    } else if (!vk->params.enable_all_features) {
        vk->features.features.robustBufferAccess = false;
        vk->vulkan_13_features.robustImageAccess = false;
        vk->vulkan_14_features.pipelineRobustness = false;
    }

    if (vk->params.require_sparse) {
        if (!vk->features.features.sparseBinding)
            vk_die("no sparse");
    } else if (!vk->params.enable_all_features) {
        vk->features.features.sparseBinding = false;
        vk->features.features.sparseResidencyBuffer = false;
        vk->features.features.sparseResidencyImage2D = false;
        vk->features.features.sparseResidencyImage3D = false;
        vk->features.features.sparseResidency2Samples = false;
        vk->features.features.sparseResidency4Samples = false;
        vk->features.features.sparseResidency8Samples = false;
        vk->features.features.sparseResidency16Samples = false;
        vk->features.features.sparseResidencyAliased = false;
    }

    if (vk->params.require_pipeline_stats) {
        if (!vk->features.features.pipelineStatisticsQuery)
            vk_die("no pipeline stats");
    } else if (!vk->params.enable_all_features) {
        vk->features.features.pipelineStatisticsQuery = false;
    }

    if (vk->params.protected_memory) {
        if (!vk->vulkan_11_features.protectedMemory)
            vk_die("no protected memory");
    } else if (!vk->params.enable_all_features) {
        vk->vulkan_11_features.protectedMemory = false;
    }

    if (vk->params.require_bda) {
        if (!vk->vulkan_12_features.bufferDeviceAddress ||
            !vk->vulkan_12_features.bufferDeviceAddressCaptureReplay)
            vk_die("no bda");
    } else if (!vk->params.enable_all_features) {
        vk->vulkan_12_features.bufferDeviceAddress = false;
        vk->vulkan_12_features.bufferDeviceAddressCaptureReplay = false;
    }

    if (vk->params.require_desc_indexing) {
        if (!vk->vulkan_12_features.descriptorIndexing)
            vk_die("no desc indexing");
    } else if (!vk->params.enable_all_features) {
        vk->vulkan_12_features.descriptorIndexing = false;
    }
}

static inline void
vk_init_physical_device_features(struct vk *vk)
{
    vk->features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    void **pnext = &vk->features.pNext;

    vk->vulkan_11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    *pnext = &vk->vulkan_11_features;
    pnext = &vk->vulkan_11_features.pNext;

    vk->vulkan_12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    *pnext = &vk->vulkan_12_features;
    pnext = &vk->vulkan_12_features.pNext;

    vk->vulkan_13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    *pnext = &vk->vulkan_13_features;
    pnext = &vk->vulkan_13_features.pNext;

    vk->vulkan_14_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
    *pnext = &vk->vulkan_14_features;
    pnext = &vk->vulkan_14_features.pNext;

    vk->external_format_resolve_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FORMAT_RESOLVE_FEATURES_ANDROID;
    *pnext = &vk->external_format_resolve_features;
    pnext = &vk->external_format_resolve_features.pNext;

    vk->custom_border_color_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT;
    *pnext = &vk->custom_border_color_features;
    pnext = &vk->custom_border_color_features.pNext;

    vk->GetPhysicalDeviceFeatures2(vk->physical_dev, &vk->features);
}

static inline void
vk_init_physical_device_properties(struct vk *vk)
{
    vk->props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    void **pnext = &vk->props.pNext;

    vk->vulkan_11_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
    *pnext = &vk->vulkan_11_props;
    pnext = &vk->vulkan_11_props.pNext;

    vk->vulkan_12_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
    *pnext = &vk->vulkan_12_props;
    pnext = &vk->vulkan_12_props.pNext;

    vk->vulkan_13_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;
    *pnext = &vk->vulkan_13_props;
    pnext = &vk->vulkan_13_props.pNext;

    vk->vulkan_14_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_PROPERTIES;
    *pnext = &vk->vulkan_14_props;
    pnext = &vk->vulkan_14_props.pNext;

    vk->external_format_resolve_props.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FORMAT_RESOLVE_PROPERTIES_ANDROID;
    *pnext = &vk->external_format_resolve_props;
    pnext = &vk->external_format_resolve_props.pNext;

    if (vk->EXT_physical_device_drm) {
        vk->drm_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT;
        *pnext = &vk->drm_props;
        pnext = &vk->drm_props.pNext;
    }

    vk->GetPhysicalDeviceProperties2(vk->physical_dev, &vk->props);
}

static inline void
vk_init_physical_device(struct vk *vk)
{
    VkPhysicalDevice physical_devs[32];
    uint32_t count = vk->params.render_node ? ARRAY_SIZE(physical_devs) : 1;
    vk->result = vk->EnumeratePhysicalDevices(vk->instance, &count, physical_devs);
    if (vk->result < VK_SUCCESS || !count) {
        vk_die("failed to enumerate physical devices: %d (no suitable icd or no dev nodes?)",
               vk->result);
    }

    for (uint32_t i = 0; i < count; i++) {
        vk->physical_dev = physical_devs[i];
        vk_init_physical_device_properties(vk);
        if (!vk->params.render_node)
            break;

        if (!vk->EXT_physical_device_drm)
            vk_die("no VK_EXT_physical_device_drm");

        struct stat sb;
        if (stat(vk->params.render_node, &sb) || !S_ISCHR(sb.st_mode))
            vk_die("bad render node %s", vk->params.render_node);
        if (makedev(vk->drm_props.primaryMajor, vk->drm_props.primaryMinor) == sb.st_rdev ||
            makedev(vk->drm_props.renderMajor, vk->drm_props.renderMinor) == sb.st_rdev)
            break;

        vk->physical_dev = NULL;
    }
    if (!vk->physical_dev)
        vk_die("failed to find the physical device for %s", vk->params.render_node);
    if (vk->props.properties.apiVersion < vk->params.api_version) {
        vk_die("physical device api version %d < %d", vk->props.properties.apiVersion,
               vk->params.api_version);
    }

    vk_init_physical_device_features(vk);
    vk_init_physical_device_feature_fixups(vk);
    vk_init_physical_device_memory_properties(vk);
}

static inline void
vk_init_device_dispatch(struct vk *vk)
{
    vk->GetDeviceProcAddr =
        (PFN_vkGetDeviceProcAddr)vk->GetInstanceProcAddr(vk->instance, "vkGetDeviceProcAddr");

#define PFN_DEVICE(name) vk->name = (PFN_vk##name)vk->GetDeviceProcAddr(vk->dev, "vk" #name);
#include "vkutil_entrypoints.inc"
}

static inline void
vk_init_device(struct vk *vk)
{
    vk->queue_family_index = 0;

    VkQueueFamilyGlobalPriorityProperties prio_props = {
        .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES,
    };
    VkQueueFamilyProperties2 queue_props = {
        .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2,
        .pNext = &prio_props,
    };
    uint32_t queue_count = 1;
    vk->GetPhysicalDeviceQueueFamilyProperties2(vk->physical_dev, &queue_count, &queue_props);
    if (!(queue_props.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT))
        vk_die("queue family 0 does not support graphics");
    if (vk->params.protected_memory &&
        !(queue_props.queueFamilyProperties.queueFlags & VK_QUEUE_PROTECTED_BIT))
        vk_die("queue family 0 does not support protected");
    if (!queue_props.queueFamilyProperties.timestampValidBits)
        vk_die("queue family 0 does not support timestamps");

    VkQueueGlobalPriority global_priority = VK_QUEUE_GLOBAL_PRIORITY_MEDIUM;
    if (vk->params.high_priority) {
        global_priority = prio_props.priorities[prio_props.priorityCount - 1];
        if (global_priority <= VK_QUEUE_GLOBAL_PRIORITY_MEDIUM)
            vk_die("queue family 0 does not support high priority");
    }

    const VkDeviceQueueCreateFlags queue_flags =
        vk->params.protected_memory ? VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT : 0;
    const float queue_priority = 1.0f;
    const VkDeviceQueueGlobalPriorityCreateInfo global_prio_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO,
        .globalPriority = global_priority,
    };
    const VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = &global_prio_info,
        .flags = queue_flags,
        .queueFamilyIndex = vk->queue_family_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };
    const VkDeviceCreateInfo dev_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &vk->features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .enabledExtensionCount = vk->params.dev_ext_count,
        .ppEnabledExtensionNames = vk->params.dev_exts,
    };
    vk->result = vk->CreateDevice(vk->physical_dev, &dev_info, NULL, &vk->dev);
    vk_check(vk, "failed to create device");

    vk_init_device_dispatch(vk);

    const VkDeviceQueueInfo2 queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
        .flags = queue_flags,
        .queueFamilyIndex = vk->queue_family_index,
    };
    vk->GetDeviceQueue2(vk->dev, &queue_info, &vk->queue);
}

static inline void
vk_init_desc_pool(struct vk *vk)
{
    const VkDescriptorPoolSize pool_sizes[] = {
        [0] = {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 32,
        },
        [1] = {
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 32,
        },
        [2] = {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 32,
        },
        [3] = {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
            .descriptorCount = 32,
        },
        [4] = {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
            .descriptorCount = 32,
        },
        [5] = {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 32,
        },
        [6] = {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 32,
        },
    };
    const VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 256,
        .poolSizeCount = ARRAY_SIZE(pool_sizes),
        .pPoolSizes = pool_sizes,
    };

    vk->result = vk->CreateDescriptorPool(vk->dev, &pool_info, NULL, &vk->desc_pool);
    vk_check(vk, "failed to create descriptor pool");
}

static inline void
vk_init_cmd_pool(struct vk *vk)
{
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = vk->queue_family_index,
    };

    vk->result = vk->CreateCommandPool(vk->dev, &pool_info, NULL, &vk->cmd_pool);
    vk_check(vk, "failed to create command pool");

    if (vk->params.protected_memory) {
        pool_info.flags |= VK_COMMAND_POOL_CREATE_PROTECTED_BIT;
        vk->result = vk->CreateCommandPool(vk->dev, &pool_info, NULL, &vk->protected_cmd_pool);
        vk_check(vk, "failed to create protected command pool");
    }
}

static inline void
vk_init_submit(struct vk *vk)
{
    const VkSemaphoreTypeCreateInfo sem_type_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = 0,
    };
    const VkSemaphoreCreateInfo sem_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &sem_type_info,
    };
    vk->result = vk->CreateSemaphore(vk->dev, &sem_info, NULL, &vk->submit.sem);
    vk_check(vk, "failed to create submit semaphore");

    vk->submit.sem_next = 1;

    static_assert(ARRAY_SIZE(vk->submit.cmds) == ARRAY_SIZE(vk->submit.sem_vals), "");
    static_assert(ARRAY_SIZE(vk->submit.cmds) == ARRAY_SIZE(vk->submit.protected_submits), "");
    vk->submit.count = ARRAY_SIZE(vk->submit.cmds);
}

static inline void
vk_init(struct vk *vk, const struct vk_init_params *params)
{
    memset(vk, 0, sizeof(*vk));

    vk_init_params(vk, params);
    vk_init_library(vk);
    vk_init_instance(vk);

    vk_init_physical_device(vk);
    vk_init_device(vk);

    vk_init_desc_pool(vk);
    vk_init_cmd_pool(vk);
    vk_init_submit(vk);

    /* avoid accessing dangling pointers */
    vk->params.instance_ext_count = 0;
    vk->params.dev_ext_count = 0;
}

static inline void
vk_cleanup(struct vk *vk)
{
    vk->DeviceWaitIdle(vk->dev);

    vk->DestroyDescriptorPool(vk->dev, vk->desc_pool, NULL);
    vk->DestroyCommandPool(vk->dev, vk->protected_cmd_pool, NULL);
    vk->DestroyCommandPool(vk->dev, vk->cmd_pool, NULL);
    vk->DestroySemaphore(vk->dev, vk->submit.sem, NULL);

    vk->DestroyDevice(vk->dev, NULL);

    vk->DestroyInstance(vk->instance, NULL);

    dlclose(vk->handle);
}

static inline VkDeviceMemory
vk_alloc_memory(struct vk *vk, VkDeviceSize size, uint32_t mt_index)
{
    const VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = size,
        .memoryTypeIndex = mt_index,
    };

    VkDeviceMemory mem;
    vk->result = vk->AllocateMemory(vk->dev, &alloc_info, NULL, &mem);
    vk_check(vk, "failed to allocate memory of size %zu\n", (size_t)size);

    return mem;
}

static inline uint32_t
vk_get_buffer_mt_mask(struct vk *vk,
                      VkBufferCreateFlags flags,
                      VkDeviceSize size,
                      VkBufferUsageFlags2 usage)
{
    const VkBufferUsageFlags2CreateInfo usage_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO,
        .usage = usage,
    };
    const struct VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = &usage_info,
        .flags = flags,
        .size = size,
    };
    VkBuffer buf;
    vk->result = vk->CreateBuffer(vk->dev, &info, NULL, &buf);
    vk_check(vk, "failed to create test buffer");

    const VkBufferMemoryRequirementsInfo2 reqs_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
        .buffer = buf,
    };
    VkMemoryRequirements2 reqs2 = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
    };
    vk->GetBufferMemoryRequirements2(vk->dev, &reqs_info, &reqs2);

    vk->DestroyBuffer(vk->dev, buf, NULL);

    return reqs2.memoryRequirements.memoryTypeBits;
}

static inline struct vk_buffer *
vk_create_buffer_with_mt(struct vk *vk,
                         VkBufferCreateFlags flags,
                         VkDeviceSize size,
                         VkBufferUsageFlags2 usage,
                         uint32_t mt_idx)
{
    struct vk_buffer *buf = (struct vk_buffer *)calloc(1, sizeof(*buf));
    if (!buf)
        vk_die("failed to alloc buf");

    buf->usage_info = (VkBufferUsageFlags2CreateInfo){
        .sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO,
        .usage = usage,
    };
    buf->info = (VkBufferCreateInfo){
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = &buf->usage_info,
        .flags = flags,
        .size = size,
    };

    vk->result = vk->CreateBuffer(vk->dev, &buf->info, NULL, &buf->buf);
    vk_check(vk, "failed to create buffer");

    const VkBufferMemoryRequirementsInfo2 reqs_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
        .buffer = buf->buf,
    };
    VkMemoryRequirements2 reqs2 = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
    };
    vk->GetBufferMemoryRequirements2(vk->dev, &reqs_info, &reqs2);
    const VkMemoryRequirements *reqs = &reqs2.memoryRequirements;
    if (!(reqs->memoryTypeBits & (1u << mt_idx)))
        vk_die("failed to meet buf memory reqs: 0x%x", reqs->memoryTypeBits);

    buf->mem = vk_alloc_memory(vk, reqs->size, mt_idx);
    buf->mem_size = reqs->size;

    const VkMemoryType *mt = &vk->mem_props.memoryTypes[mt_idx];
    if (mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        const VkMemoryMapInfo map_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_MAP_INFO,
            .memory = buf->mem,
            .size = buf->mem_size,
        };
        vk->result = vk->MapMemory2(vk->dev, &map_info, &buf->mem_ptr);
        vk_check(vk, "failed to map buffer memory");

        buf->is_coherent = mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }

    const VkBindBufferMemoryInfo bind_info = {
        .sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
        .buffer = buf->buf,
        .memory = buf->mem,
    };
    vk->result = vk->BindBufferMemory2(vk->dev, 1, &bind_info);
    vk_check(vk, "failed to bind buffer memory");

    return buf;
}

static inline struct vk_buffer *
vk_create_buffer(struct vk *vk,
                 VkBufferCreateFlags flags,
                 VkDeviceSize size,
                 VkBufferUsageFlags2 usage)
{
    return vk_create_buffer_with_mt(vk, flags, size, usage, vk->buf_mt_index);
}

static inline void
vk_destroy_buffer(struct vk *vk, struct vk_buffer *buf)
{
    vk->FreeMemory(vk->dev, buf->mem, NULL);
    vk->DestroyBuffer(vk->dev, buf->buf, NULL);
    free(buf);
}

static inline VkFormatFeatureFlags2
vk_validate_image_info(struct vk *vk, const VkImageCreateInfo *info)
{
    VkFormatProperties3 fmt_props3 = {
        .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3,
    };
    VkFormatProperties2 fmt_props = {
        .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
        .pNext = &fmt_props3,
    };
    vk->GetPhysicalDeviceFormatProperties2(vk->physical_dev, info->format, &fmt_props);
    const VkFormatFeatureFlags2 features = info->tiling == VK_IMAGE_TILING_OPTIMAL
                                               ? fmt_props3.optimalTilingFeatures
                                               : fmt_props3.linearTilingFeatures;

    const struct {
        VkImageUsageFlagBits usage;
        VkFormatFeatureFlags2 feature;
    } pairs[] = {
        { VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_FORMAT_FEATURE_2_TRANSFER_SRC_BIT },
        { VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_FORMAT_FEATURE_2_TRANSFER_DST_BIT },
        { VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT },
        { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT },
        { VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
          VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT },
    };

    for (uint32_t i = 0; i < ARRAY_SIZE(pairs); i++) {
        if ((info->usage & pairs[i].usage) && !(features & pairs[i].feature))
            vk_die("image usage 0x%x is not supported", info->usage);
    }

    const VkPhysicalDeviceImageFormatInfo2 fmt_info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
        .format = info->format,
        .type = info->imageType,
        .tiling = info->tiling,
        .usage = info->usage,
        .flags = info->flags,
    };
    VkImageFormatProperties2 fmt_props2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
    };
    vk->result =
        vk->GetPhysicalDeviceImageFormatProperties2(vk->physical_dev, &fmt_info, &fmt_props2);
    vk_check(vk, "image format/type/tiling/usage/flags is not supported");

    const VkImageFormatProperties *img_props = &fmt_props2.imageFormatProperties;

    if (info->extent.width > img_props->maxExtent.width)
        vk_die("image width %u is not supported", info->extent.width);
    if (info->extent.height > img_props->maxExtent.height)
        vk_die("image height %u is not supported", info->extent.height);
    if (info->extent.depth > img_props->maxExtent.depth)
        vk_die("image depth %u is not supported", info->extent.depth);
    if (info->mipLevels > img_props->maxMipLevels)
        vk_die("image miplevel %u is not supported", info->mipLevels);
    if (info->arrayLayers > img_props->maxArrayLayers)
        vk_die("image array layer %u is not supported", info->arrayLayers);
    if (!(info->samples & img_props->sampleCounts))
        vk_die("image sample count %u is not supported", info->samples);

    return features;
}

static inline uint32_t
vk_get_image_mt_mask(struct vk *vk, const VkImageCreateInfo *info)
{
    vk_validate_image_info(vk, info);

    VkImage img;
    vk->result = vk->CreateImage(vk->dev, info, NULL, &img);
    vk_check(vk, "failed to create test image");

    const VkImageMemoryRequirementsInfo2 reqs_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
        .image = img,
    };
    VkMemoryRequirements2 reqs2 = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
    };
    vk->GetImageMemoryRequirements2(vk->dev, &reqs_info, &reqs2);

    vk->DestroyImage(vk->dev, img, NULL);

    return reqs2.memoryRequirements.memoryTypeBits;
}

static inline void
vk_init_image(struct vk *vk, struct vk_image *img, uint32_t mt_idx)
{
    img->features = vk_validate_image_info(vk, &img->info);

    vk->result = vk->CreateImage(vk->dev, &img->info, NULL, &img->img);
    vk_check(vk, "failed to create image");

    const VkImageMemoryRequirementsInfo2 reqs_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
        .image = img->img,
    };
    VkMemoryRequirements2 reqs2 = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
    };
    vk->GetImageMemoryRequirements2(vk->dev, &reqs_info, &reqs2);
    const VkMemoryRequirements *reqs = &reqs2.memoryRequirements;

    if (mt_idx == VK_MAX_MEMORY_TYPES) {
        if (reqs->memoryTypeBits & (1u << vk->buf_mt_index))
            mt_idx = vk->buf_mt_index;
        else
            mt_idx = ffs(reqs->memoryTypeBits) - 1;
    }

    img->mem = vk_alloc_memory(vk, reqs->size, mt_idx);
    img->mem_size = reqs->size;

    const VkMemoryType *mt = &vk->mem_props.memoryTypes[mt_idx];
    if (mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        const VkMemoryMapInfo map_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_MAP_INFO,
            .memory = img->mem,
            .size = img->mem_size,
        };
        vk->result = vk->MapMemory2(vk->dev, &map_info, &img->mem_ptr);
        vk_check(vk, "failed to map image memory");

        img->is_coherent = mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }

    const VkBindImageMemoryInfo bind_info = {
        .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
        .image = img->img,
        .memory = img->mem,
    };
    vk->result = vk->BindImageMemory2(vk->dev, 1, &bind_info);
    vk_check(vk, "failed to bind image memory");
}

static inline struct vk_image *
vk_create_image_with_mt(struct vk *vk, const VkImageCreateInfo *info, uint32_t mt_idx)
{
    struct vk_image *img = (struct vk_image *)calloc(1, sizeof(*img));
    if (!img)
        vk_die("failed to alloc img");

    img->info = *info;
    vk_init_image(vk, img, mt_idx);

    return img;
}

static inline struct vk_image *
vk_create_image_from_info(struct vk *vk, const VkImageCreateInfo *info)
{
    return vk_create_image_with_mt(vk, info, VK_MAX_MEMORY_TYPES);
}

static inline struct vk_image *
vk_create_image(struct vk *vk,
                VkFormat format,
                uint32_t width,
                uint32_t height,
                VkSampleCountFlagBits samples,
                VkImageTiling tiling,
                VkImageUsageFlags usage)
{
    struct vk_image *img = (struct vk_image *)calloc(1, sizeof(*img));
    if (!img)
        vk_die("failed to alloc img");

    const VkImageCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {
            .width = width,
            .height = height,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = samples,
        .tiling = tiling,
        .usage = usage,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    return vk_create_image_from_info(vk, &info);
}

static inline struct vk_image *
vk_create_image_from_ppm(struct vk *vk, const void *ppm_data, size_t ppm_size, bool planar)
{
    uint32_t width;
    uint32_t height;
    ppm_data = u_parse_ppm(ppm_data, ppm_size, &width, &height);

    const VkFormat fmt = planar ? VK_FORMAT_G8_B8R8_2PLANE_420_UNORM : VK_FORMAT_B8G8R8A8_UNORM;

    struct vk_image *img = (struct vk_image *)calloc(1, sizeof(*img));
    if (!img)
        vk_die("failed to alloc img");

    img->info = (VkImageCreateInfo){
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = fmt,
            .extent = {
                .width = width,
                .height = height,
                .depth = 1,
            },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_LINEAR,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
    };

    vk_init_image(vk, img, VK_MAX_MEMORY_TYPES);

    struct u_format_conversion conv = {
        .width = width,
        .height = height,

        .src_format = DRM_FORMAT_BGR888,
        .src_plane_count = 1,
        .src_plane_ptrs = { ppm_data, },
        .src_plane_strides = { width * 3, },

        .dst_format = planar ? DRM_FORMAT_NV12 : DRM_FORMAT_ABGR8888,
        .dst_plane_count = (uint32_t)(planar ? 2 : 1),
    };
    if (planar) {
        const VkImageSubresource2 y_subres2 = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_SUBRESOURCE_2,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT,
            },
        };
        const VkImageSubresource2 uv_subres2 = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_SUBRESOURCE_2,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT,
            },
        };
        VkSubresourceLayout2 y_layout2 = {
            .sType = VK_STRUCTURE_TYPE_SUBRESOURCE_LAYOUT_2,
        };
        VkSubresourceLayout2 uv_layout2 = {
            .sType = VK_STRUCTURE_TYPE_SUBRESOURCE_LAYOUT_2,
        };
        vk->GetImageSubresourceLayout2(vk->dev, img->img, &y_subres2, &y_layout2);
        vk->GetImageSubresourceLayout2(vk->dev, img->img, &uv_subres2, &uv_layout2);

        conv.dst_plane_ptrs[0] = (uint8_t *)img->mem_ptr + y_layout2.subresourceLayout.offset;
        conv.dst_plane_strides[0] = y_layout2.subresourceLayout.rowPitch;
        conv.dst_plane_ptrs[1] = (uint8_t *)img->mem_ptr + uv_layout2.subresourceLayout.offset;
        conv.dst_plane_strides[1] = uv_layout2.subresourceLayout.rowPitch;
    } else {
        const VkImageSubresource2 subres2 = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_SUBRESOURCE_2,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            },
        };
        VkSubresourceLayout2 layout2 = {
            .sType = VK_STRUCTURE_TYPE_SUBRESOURCE_LAYOUT_2,
        };
        vk->GetImageSubresourceLayout2(vk->dev, img->img, &subres2, &layout2);

        conv.dst_plane_ptrs[0] = (uint8_t *)img->mem_ptr + layout2.subresourceLayout.offset;
        conv.dst_plane_strides[0] = layout2.subresourceLayout.rowPitch;
    }

    u_convert_format(&conv);

    return img;
}

static inline void
vk_create_image_render_view(struct vk *vk, struct vk_image *img, VkImageAspectFlags aspect_mask)
{
    const VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = img->img,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = img->info.format,
        .subresourceRange = {
            .aspectMask = aspect_mask,
            .levelCount = img->info.mipLevels,
            .layerCount = img->info.arrayLayers,
        },
    };
    vk->result = vk->CreateImageView(vk->dev, &view_info, NULL, &img->render_view);
    vk_check(vk, "failed to create image render view");
}

static inline void
vk_create_image_ycbcr_conversion(struct vk *vk,
                                 struct vk_image *img,
                                 VkChromaLocation chroma_offset,
                                 VkFilter chroma_filter)
{
    if (chroma_offset == VK_CHROMA_LOCATION_MIDPOINT &&
        !(img->features & VK_FORMAT_FEATURE_2_MIDPOINT_CHROMA_SAMPLES_BIT))
        vk_die("image does not support midpoint chroma offset");
    else if (chroma_offset == VK_CHROMA_LOCATION_COSITED_EVEN &&
             !(img->features & VK_FORMAT_FEATURE_2_COSITED_CHROMA_SAMPLES_BIT))
        vk_die("image does not support cosited chroma offset");

    if (chroma_filter == VK_FILTER_LINEAR &&
        !(img->features & VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT))
        vk_die("image does not support linear chroma offset");

    const VkPhysicalDeviceImageFormatInfo2 fmt_info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
        .format = img->info.format,
        .type = img->info.imageType,
        .tiling = img->info.tiling,
        .usage = img->info.usage,
    };
    VkSamplerYcbcrConversionImageFormatProperties ycbcr_props = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES,
    };
    VkImageFormatProperties2 fmt_props = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
        .pNext = &ycbcr_props,
    };
    vk->result =
        vk->GetPhysicalDeviceImageFormatProperties2(vk->physical_dev, &fmt_info, &fmt_props);
    vk_check(vk, "unsupported VkSamplerYcbcrConversion format");

    const VkSamplerYcbcrConversionCreateInfo conv_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
        .format = img->info.format,
        .ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601,
        .ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_FULL,
        .xChromaOffset = chroma_offset,
        .yChromaOffset = chroma_offset,
        .chromaFilter = chroma_filter,
        .forceExplicitReconstruction = false,
    };

    VkSamplerYcbcrConversion conv;
    vk->result = vk->CreateSamplerYcbcrConversion(vk->dev, &conv_info, NULL, &conv);
    vk_check(vk, "failed to create VkSamplerYcbcrConversion");

    img->ycbcr_conv = conv;
    img->ycbcr_conv_desc_count = ycbcr_props.combinedImageSamplerDescriptorCount;
}

static inline void
vk_create_image_sample_view(struct vk *vk,
                            struct vk_image *img,
                            VkImageViewType type,
                            VkImageAspectFlagBits aspect)
{
    const VkSamplerYcbcrConversionInfo conv_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
        .conversion = img->ycbcr_conv,
    };

    const VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = img->ycbcr_conv != VK_NULL_HANDLE ? &conv_info : NULL,
        .image = img->img,
        .viewType = type,
        .format = img->info.format,
        .subresourceRange = {
            .aspectMask = aspect,
            .levelCount = img->info.mipLevels,
            .layerCount = img->info.arrayLayers,
        },
    };
    vk->result = vk->CreateImageView(vk->dev, &view_info, NULL, &img->sample_view);
    vk_check(vk, "failed to create image sample view");

    img->sample_view_type = type;
}

static inline void
vk_create_image_sampler(struct vk *vk,
                        struct vk_image *img,
                        VkFilter filter,
                        VkSamplerMipmapMode mipmap_mode)
{
    const VkSamplerYcbcrConversionInfo conv_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
        .conversion = img->ycbcr_conv,
    };
    const VkSamplerCustomBorderColorCreateInfoEXT border_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT,
        .customBorderColor = { .uint32 = { 10, 0, 0, 0 }, },
        .format = img->info.format,
    };

    VkSamplerAddressMode addr_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    VkBorderColor border_color = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    void *pnext = NULL;
    if (img->ycbcr_conv != VK_NULL_HANDLE) {
        addr_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        pnext = (void *)&conv_info;
    } else if (vk->EXT_custom_border_color) {
        border_color = VK_BORDER_COLOR_INT_CUSTOM_EXT;
        pnext = (void *)&border_info;
    }

    const VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = pnext,
        .magFilter = filter,
        .minFilter = filter,
        .mipmapMode = mipmap_mode,
        .addressModeU = addr_mode,
        .addressModeV = addr_mode,
        .addressModeW = addr_mode,
        .borderColor = border_color,
    };
    vk->result = vk->CreateSampler(vk->dev, &sampler_info, NULL, &img->sampler);
    vk_check(vk, "failed to create sampler");
}

static inline void
vk_destroy_image(struct vk *vk, struct vk_image *img)
{
    vk->DestroySampler(vk->dev, img->sampler, NULL);
    vk->DestroyImageView(vk->dev, img->sample_view, NULL);

    vk->DestroySamplerYcbcrConversion(vk->dev, img->ycbcr_conv, NULL);

    vk->DestroyImageView(vk->dev, img->render_view, NULL);

    vk->FreeMemory(vk->dev, img->mem, NULL);
    vk->DestroyImage(vk->dev, img->img, NULL);
    free(img);
}

static inline void
vk_fill_image(struct vk *vk, struct vk_image *img, uint8_t val)
{
    if (!img->mem_ptr)
        vk_die("cannot fill non-mappable image");

    if (img->info.tiling != VK_IMAGE_TILING_LINEAR)
        vk_log("filling non-linear image");

    memset(img->mem_ptr, val, img->mem_size);
}

static inline void
vk_write_ppm(const char *filename,
             const void *data,
             VkFormat format,
             uint32_t width,
             uint32_t height,
             VkDeviceSize pitch)
{
    uint8_t swizzle[3];
    uint32_t cpp;
    uint16_t max_val;
    bool packed;
    switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM:
        cpp = 4;
        max_val = 255;
        packed = false;
        swizzle[0] = 0;
        swizzle[1] = 1;
        swizzle[2] = 2;
        break;
    case VK_FORMAT_B8G8R8A8_UNORM:
        cpp = 4;
        max_val = 255;
        packed = false;
        swizzle[0] = 2;
        swizzle[1] = 1;
        swizzle[2] = 0;
        break;
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
        cpp = 2;
        max_val = 31;
        packed = true;
        swizzle[0] = 2;
        swizzle[1] = 1;
        swizzle[2] = 0;
        break;
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
        cpp = 2;
        max_val = 31;
        packed = true;
        swizzle[0] = 2;
        swizzle[1] = 1;
        swizzle[2] = 0;
        break;
    case VK_FORMAT_R32G32B32A32_UINT:
        cpp = 16;
        max_val = 255;
        packed = false;
        swizzle[0] = 0;
        swizzle[1] = 1;
        swizzle[2] = 2;
        break;
    default:
        vk_die("cannot write unknown format %d", format);
        break;
    }

    FILE *fp = fopen(filename, "w");
    if (!fp)
        vk_die("failed to open %s", filename);

    fprintf(fp, "P6 %u %u %u\n", width, height, max_val);
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            if (format == VK_FORMAT_R32G32B32A32_UINT) {
                const uint32_t *pixel =
                    (const uint32_t *)((const uint8_t *)data + pitch * y + cpp * x);
                /* discard the higher bytes */
                const char bytes[3] = { (char)pixel[swizzle[0]], (char)pixel[swizzle[1]],
                                        (char)pixel[swizzle[2]] };
                if (fwrite(bytes, sizeof(bytes), 1, fp) != 1)
                    vk_die("failed to write pixel (%u, %u)", x, y);
            } else if (packed) {
                const uint16_t *pixel =
                    (const uint16_t *)((const uint8_t *)data + pitch * y + cpp * x);
                uint16_t val = *pixel;
                if (format == VK_FORMAT_R5G5B5A1_UNORM_PACK16)
                    val >>= 1;

                const char comps[3] = { (char)(val & 0x1f), (char)((val >> 5) & 0x1f),
                                        (char)((val >> 10) & 0x1f) };
                const char bytes[3] = { comps[swizzle[0]], comps[swizzle[1]], comps[swizzle[2]] };
                if (fwrite(bytes, sizeof(bytes), 1, fp) != 1)
                    vk_die("failed to write pixel (%u, %u)", x, y);
            } else {
                const char *pixel = (const char *)((const uint8_t *)data + pitch * y + cpp * x);
                const char bytes[3] = { pixel[swizzle[0]], pixel[swizzle[1]], pixel[swizzle[2]] };
                if (fwrite(bytes, sizeof(bytes), 1, fp) != 1)
                    vk_die("failed to write pixel (%u, %u)", x, y);
            }
        }
    }

    fclose(fp);
}

static inline void
vk_dump_image(struct vk *vk,
              struct vk_image *img,
              VkImageAspectFlagBits aspect,
              const char *filename)
{
    if (!img->mem_ptr)
        vk_die("cannot dump non-mappable image");

    if (img->info.tiling != VK_IMAGE_TILING_LINEAR)
        vk_log("dumping non-linear image");

    if (img->info.samples != VK_SAMPLE_COUNT_1_BIT)
        vk_log("dumping msaa image");

    const VkImageSubresource2 subres2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_SUBRESOURCE_2,
        .imageSubresource = {
            .aspectMask = aspect,
        },
    };
    VkSubresourceLayout2 layout2 = {
        .sType = VK_STRUCTURE_TYPE_SUBRESOURCE_LAYOUT_2,
    };
    vk->GetImageSubresourceLayout2(vk->dev, img->img, &subres2, &layout2);

    vk_write_ppm(filename, (const uint8_t *)img->mem_ptr + layout2.subresourceLayout.offset,
                 img->info.format, img->info.extent.width * img->info.samples,
                 img->info.extent.height, layout2.subresourceLayout.rowPitch);
}

static inline void
vk_dump_image_raw(struct vk *vk, struct vk_image *img, const char *filename)
{
    if (!img->mem_ptr)
        vk_die("cannot dump non-mappable image");

    FILE *fp = fopen(filename, "w");
    if (!fp)
        vk_die("failed to open %s", filename);
    if (fwrite(img->mem_ptr, 1, img->mem_size, fp) != img->mem_size)
        vk_die("failed to write raw memory");
    fclose(fp);
}

static inline void
vk_dump_buffer_raw(struct vk *vk,
                   struct vk_buffer *buf,
                   VkDeviceSize offset,
                   VkDeviceSize size,
                   const char *filename)
{
    if (size) {
        if (offset >= buf->mem_size)
            vk_die("bad dump offset");

        if (size == VK_WHOLE_SIZE)
            size = buf->mem_size - offset;

        if (size > buf->mem_size - offset)
            vk_die("bad dump size");
    } else {
        offset = 0;
    }

    FILE *fp = fopen(filename, "w");
    if (!fp)
        vk_die("failed to open %s", filename);
    if (fwrite((const uint8_t *)buf->mem_ptr + offset, 1, size, fp) != size)
        vk_die("failed to write raw memory");
    fclose(fp);
}

static inline struct vk_pipeline *
vk_create_pipeline(struct vk *vk)
{
    struct vk_pipeline *pipeline = (struct vk_pipeline *)calloc(1, sizeof(*pipeline));
    if (!pipeline)
        vk_die("failed to alloc pipeline");

    pipeline->topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    pipeline->poly_mode = VK_POLYGON_MODE_FILL;
    pipeline->sample_count = VK_SAMPLE_COUNT_1_BIT;

    pipeline->depth_info = (VkPipelineDepthStencilStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    };

    return pipeline;
}

static inline void
vk_add_pipeline_shader(struct vk *vk,
                       struct vk_pipeline *pipeline,
                       VkShaderStageFlagBits stage,
                       const uint32_t *code,
                       size_t size)
{
    const uint32_t idx = pipeline->stage_count++;
    pipeline->mods[idx] = (VkShaderModuleCreateInfo){
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = code,
    };
    pipeline->stages[idx] = (VkPipelineShaderStageCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = &pipeline->mods[idx],
        .stage = stage,
        .pName = "main",
    };
}

static inline void
vk_add_pipeline_set_layout_from_info(struct vk *vk,
                                     struct vk_pipeline *pipeline,
                                     const VkDescriptorSetLayoutCreateInfo *create_info)
{
    assert(pipeline->set_layout_count < ARRAY_SIZE(pipeline->set_layouts));

    vk->result = vk->CreateDescriptorSetLayout(
        vk->dev, create_info, NULL, &pipeline->set_layouts[pipeline->set_layout_count++]);
    vk_check(vk, "failed to create descriptor set layout");
}

static inline void
vk_add_pipeline_set_layout(struct vk *vk,
                           struct vk_pipeline *pipeline,
                           VkDescriptorType type,
                           uint32_t desc_count,
                           VkShaderStageFlags stages,
                           const VkSampler *immutable_samplers)
{
    const VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = type,
        .descriptorCount = desc_count,
        .stageFlags = stages,
        .pImmutableSamplers = immutable_samplers,
    };
    const VkDescriptorSetLayoutCreateInfo set_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding,
    };

    vk_add_pipeline_set_layout_from_info(vk, pipeline, &set_layout_info);
}

static inline void
vk_set_pipeline_vertices(struct vk *vk,
                         struct vk_pipeline *pipeline,
                         const uint32_t *comp_counts,
                         uint32_t attr_count)
{
    assert(attr_count < ARRAY_SIZE(pipeline->vi_attrs));

    uint32_t offset = 0;
    for (uint32_t i = 0; i < attr_count; i++) {
        VkFormat format;
        switch (comp_counts[i]) {
        case 1:
            format = VK_FORMAT_R32_SFLOAT;
            break;
        case 2:
            format = VK_FORMAT_R32G32_SFLOAT;
            break;
        case 3:
            format = VK_FORMAT_R32G32B32_SFLOAT;
            break;
        case 4:
            format = VK_FORMAT_R32G32B32A32_SFLOAT;
            break;
        default:
            vk_die("unsupported vertex attribute format %d", comp_counts[i]);
            break;
        }

        pipeline->vi_attrs[i] = (VkVertexInputAttributeDescription){
            .location = i,
            .binding = 0,
            .format = format,
            .offset = offset,
        };
        offset += sizeof(float) * comp_counts[i];
    }

    pipeline->vi_attr_count = attr_count;

    pipeline->vi_binding = (VkVertexInputBindingDescription){
        .binding = 0,
        .stride = offset,
    };
}

static inline void
vk_set_pipeline_viewport(struct vk *vk,
                         struct vk_pipeline *pipeline,
                         uint32_t width,
                         uint32_t height)
{
    pipeline->viewport = (VkViewport){
        .width = (float)width,
        .height = (float)height,
        .maxDepth = 1.0f,
    };

    pipeline->scissor = (VkRect2D){
        .extent = {
            .width = width,
            .height = height,
        },
    };
}

static inline void
vk_compile_pipeline(struct vk *vk, struct vk_pipeline *pipeline)
{
    const VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = pipeline->set_layout_count,
        .pSetLayouts = pipeline->set_layouts,
        .pushConstantRangeCount = (uint32_t)(pipeline->push_const.size ? 1 : 0),
        .pPushConstantRanges = &pipeline->push_const,
    };
    vk->result =
        vk->CreatePipelineLayout(vk->dev, &pipeline_layout_info, NULL, &pipeline->layout);
    vk_check(vk, "failed to create pipeline layout");

    const VkPipelineCreateFlags2CreateInfo flags2_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO,
        .flags = pipeline->flags2,
    };

    if (pipeline->stage_count == 1 && pipeline->stages[0].stage == VK_SHADER_STAGE_COMPUTE_BIT) {
        const VkComputePipelineCreateInfo compute_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = &flags2_info,
            .stage = pipeline->stages[0],
            .layout = pipeline->layout,
        };
        vk->result = vk->CreateComputePipelines(vk->dev, VK_NULL_HANDLE, 1, &compute_info, NULL,
                                                &pipeline->pipeline);
        vk_check(vk, "failed to create compute pipeline");
        return;
    }

    const VkPipelineVertexInputStateCreateInfo vi_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = pipeline->vi_attr_count ? 1u : 0u,
        .pVertexBindingDescriptions = &pipeline->vi_binding,
        .vertexAttributeDescriptionCount = pipeline->vi_attr_count,
        .pVertexAttributeDescriptions = pipeline->vi_attrs,
    };

    const VkPipelineInputAssemblyStateCreateInfo ia_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    };

    const VkPipelineTessellationStateCreateInfo tess_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
        .patchControlPoints = pipeline->patch_control_points,
    };

    const VkPipelineViewportStateCreateInfo vp_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    };

    const VkPipelineRasterizationStateCreateInfo rast_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = pipeline->poly_mode,
    };

    const VkSampleMask sample_mask = (1u << pipeline->sample_count) - 1;
    const VkPipelineMultisampleStateCreateInfo msaa_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = pipeline->sample_count,
        .pSampleMask = &sample_mask,
    };

    const VkPipelineColorBlendAttachmentState blend_att = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    const VkPipelineColorBlendStateCreateInfo blend_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_att,
    };

    const VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY, VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
        VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT, VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE,
        VK_DYNAMIC_STATE_CULL_MODE,          VK_DYNAMIC_STATE_FRONT_FACE,
        VK_DYNAMIC_STATE_LINE_WIDTH,
    };
    const VkPipelineDynamicStateCreateInfo dynamic_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = ARRAY_SIZE(dynamic_states),
        .pDynamicStates = dynamic_states,
    };

    const uint32_t color_att_count =
        pipeline->color_att_format != VK_FORMAT_UNDEFINED || pipeline->external_format != 0;
    const VkExternalFormatANDROID ext_fmt = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID,
        .pNext = (void *)&flags2_info,
        .externalFormat = pipeline->external_format,
    };
    const VkPipelineRenderingCreateInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = pipeline->external_format ? (const void *)&ext_fmt : (const void *)&flags2_info,
        .colorAttachmentCount = color_att_count,
        .pColorAttachmentFormats = &pipeline->color_att_format,
        .depthAttachmentFormat = pipeline->depth_att_format,
        .stencilAttachmentFormat = pipeline->stencil_att_format,
    };

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering_info,
        .stageCount = pipeline->stage_count,
        .pStages = pipeline->stages,
        .pVertexInputState = &vi_info,
        .pInputAssemblyState = &ia_info,
        .pTessellationState = &tess_info,
        .pViewportState = &vp_info,
        .pRasterizationState = &rast_info,
        .pMultisampleState = &msaa_info,
        .pDepthStencilState = &pipeline->depth_info,
        .pColorBlendState = &blend_info,
        .pDynamicState = &dynamic_info,
        .layout = pipeline->layout,
    };

    vk->result = vk->CreateGraphicsPipelines(vk->dev, VK_NULL_HANDLE, 1, &pipeline_info, NULL,
                                             &pipeline->pipeline);
    vk_check(vk, "failed to create graphics pipeline");
}

static inline void
vk_bind_pipeline(struct vk *vk, const struct vk_pipeline *pipeline, VkCommandBuffer cmd)
{
    if (pipeline->stage_count == 1 && pipeline->stages[0].stage == VK_SHADER_STAGE_COMPUTE_BIT) {
        vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);
        return;
    }

    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);

    vk->CmdSetPrimitiveTopology(cmd, pipeline->topology);

    vk->CmdSetViewportWithCount(cmd, 1, &pipeline->viewport);
    vk->CmdSetScissorWithCount(cmd, 1, &pipeline->scissor);

    vk->CmdSetRasterizerDiscardEnable(cmd, pipeline->rasterizer_discard);
    vk->CmdSetCullMode(cmd, VK_CULL_MODE_NONE);
    vk->CmdSetFrontFace(cmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    vk->CmdSetLineWidth(cmd, 1.0f);
}

static inline void
vk_destroy_pipeline(struct vk *vk, struct vk_pipeline *pipeline)
{
    for (uint32_t i = 0; i < pipeline->set_layout_count; i++)
        vk->DestroyDescriptorSetLayout(vk->dev, pipeline->set_layouts[i], NULL);

    vk->DestroyPipelineLayout(vk->dev, pipeline->layout, NULL);

    vk->DestroyPipeline(vk->dev, pipeline->pipeline, NULL);

    free(pipeline);
}

static inline struct vk_descriptor_set *
vk_create_descriptor_set(struct vk *vk, VkDescriptorSetLayout layout)
{
    struct vk_descriptor_set *set = (struct vk_descriptor_set *)calloc(1, sizeof(*set));
    if (!set)
        vk_die("failed to alloc set");

    const VkDescriptorSetAllocateInfo set_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = vk->desc_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    vk->result = vk->AllocateDescriptorSets(vk->dev, &set_info, &set->set);
    vk_check(vk, "failed to allocate descriptor set");

    return set;
}

static inline void
vk_write_descriptor_set_buffer(struct vk *vk,
                               struct vk_descriptor_set *set,
                               VkDescriptorType type,
                               const struct vk_buffer *buf,
                               VkDeviceSize size)
{
    const VkDescriptorBufferInfo buf_info = {
        .buffer = buf->buf,
        .offset = 0,
        .range = size,
    };
    const VkWriteDescriptorSet write_info = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set->set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = type,
        .pBufferInfo = &buf_info,
    };

    vk->UpdateDescriptorSets(vk->dev, 1, &write_info, 0, NULL);
}

static inline void
vk_write_descriptor_set_image(struct vk *vk,
                              struct vk_descriptor_set *set,
                              const struct vk_image *img)
{
    const VkDescriptorImageInfo img_info = {
        .sampler = img->sampler,
        .imageView = img->sample_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    const VkWriteDescriptorSet write_info = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set->set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &img_info,
    };

    vk->UpdateDescriptorSets(vk->dev, 1, &write_info, 0, NULL);
}

static inline void
vk_destroy_descriptor_set(struct vk *vk, struct vk_descriptor_set *set)
{
    free(set);
}

static inline struct vk_semaphore *
vk_create_semaphore(struct vk *vk,
                    VkSemaphoreType type,
                    VkExternalSemaphoreHandleTypeFlags handle_type)
{
    struct vk_semaphore *sem = (struct vk_semaphore *)calloc(1, sizeof(*sem));
    if (!sem)
        vk_die("failed to alloc semaphore");

    if (type == VK_SEMAPHORE_TYPE_TIMELINE) {
        if (handle_type == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT)
            vk_die("timeline sync fd is invalid");
    }
    if (handle_type & (handle_type - 1))
        vk_die("must be a single handle type");

    sem->type = type;
    sem->handle_type = (VkExternalSemaphoreHandleTypeFlagBits)handle_type;

    const VkExportSemaphoreCreateInfo export_info = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
        .handleTypes = handle_type,
    };
    const VkSemaphoreTypeCreateInfo type_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .pNext = handle_type ? &export_info : NULL,
        .semaphoreType = type,
        .initialValue = 0,
    };
    const VkSemaphoreCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &type_info,
    };

    vk->result = vk->CreateSemaphore(vk->dev, &info, NULL, &sem->sem);
    vk_check(vk, "failed to create semaphore");

    return sem;
}

static inline void
vk_destroy_semaphore(struct vk *vk, struct vk_semaphore *sem)
{
    vk->DestroySemaphore(vk->dev, sem->sem, NULL);
    free(sem);
}

static inline int
vk_export_semaphore_fd(struct vk *vk, struct vk_semaphore *sem)
{
    const VkSemaphoreGetFdInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR,
        .semaphore = sem->sem,
        .handleType = sem->handle_type,
    };

    int fd;
    vk->result = vk->GetSemaphoreFdKHR(vk->dev, &info, &fd);
    vk_check(vk, "failed to export semaphore fd");

    return fd;
}

static inline void
vk_import_semaphore_fd(struct vk *vk, struct vk_semaphore *sem, int fd)
{
    const VkSemaphoreImportFlags flags =
        sem->handle_type == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT
            ? VK_SEMAPHORE_IMPORT_TEMPORARY_BIT
            : 0;
    const VkImportSemaphoreFdInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR,
        .semaphore = sem->sem,
        .flags = flags,
        .handleType = sem->handle_type,
        .fd = fd,
    };

    vk->result = vk->ImportSemaphoreFdKHR(vk->dev, &info);
    vk_check(vk, "failed to import semaphore fd");
}

static inline uint64_t
vk_get_semaphore_counter_value(struct vk *vk, struct vk_semaphore *sem)
{
    uint64_t val;
    vk->result = vk->GetSemaphoreCounterValue(vk->dev, sem->sem, &val);
    vk_check(vk, "failed to get semaphore counter value");

    return val;
}

static inline struct vk_event *
vk_create_event(struct vk *vk)
{
    struct vk_event *ev = (struct vk_event *)calloc(1, sizeof(*ev));
    if (!ev)
        vk_die("failed to alloc event");

    const VkEventCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
    };

    vk->result = vk->CreateEvent(vk->dev, &info, NULL, &ev->event);
    vk_check(vk, "failed to create event");

    return ev;
}

static inline void
vk_destroy_event(struct vk *vk, struct vk_event *ev)
{
    vk->DestroyEvent(vk->dev, ev->event, NULL);
    free(ev);
}

static inline struct vk_query *
vk_create_query(struct vk *vk, VkQueryType type, uint32_t count)
{
    struct vk_query *query = (struct vk_query *)calloc(1, sizeof(*query));
    if (!query)
        vk_die("failed to alloc query");

    const VkQueryPoolCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = type,
        .queryCount = count,
        .pipelineStatistics =
            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT,
    };

    vk->result = vk->CreateQueryPool(vk->dev, &info, NULL, &query->pool);
    vk_check(vk, "failed to create query");

    return query;
}

static inline void
vk_destroy_query(struct vk *vk, struct vk_query *query)
{
    vk->DestroyQueryPool(vk->dev, query->pool, NULL);
    free(query);
}

static inline struct vk_stopwatch *
vk_create_stopwatch(struct vk *vk, uint32_t count)
{
    struct vk_stopwatch *stopwatch = (struct vk_stopwatch *)calloc(1, sizeof(*stopwatch));
    if (!stopwatch)
        vk_die("failed to alloc stopwatch");

    stopwatch->query = vk_create_query(vk, VK_QUERY_TYPE_TIMESTAMP, count);
    stopwatch->query_max = count;
    stopwatch->query_count = 0;

    return stopwatch;
}

static inline void
vk_destroy_stopwatch(struct vk *vk, struct vk_stopwatch *stopwatch)
{
    free(stopwatch->ts);
    vk_destroy_query(vk, stopwatch->query);
    free(stopwatch);
}

static inline void
vk_reset_stopwatch(struct vk *vk, struct vk_stopwatch *stopwatch)
{
    stopwatch->query_count = 0;
    if (stopwatch->ts) {
        free(stopwatch->ts);
        stopwatch->ts = NULL;
    }
}

static inline void
vk_write_stopwatch(struct vk *vk, struct vk_stopwatch *stopwatch, VkCommandBuffer cmd)
{
    if (stopwatch->query_count >= stopwatch->query_max)
        vk_die("not enough queries");
    if (stopwatch->ts)
        vk_die("cannot write anymore");

    if (!stopwatch->query_count)
        vk->CmdResetQueryPool(cmd, stopwatch->query->pool, 0, stopwatch->query_max);

    vk->CmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, stopwatch->query->pool,
                           stopwatch->query_count++);
}

static inline uint64_t
vk_read_stopwatch(struct vk *vk, struct vk_stopwatch *stopwatch, uint32_t idx)
{
    if (!stopwatch->ts) {
        stopwatch->ts = (uint64_t *)malloc(sizeof(*stopwatch->ts) * stopwatch->query_count);
        if (!stopwatch->ts)
            vk_die("failed to alloc ts");

        vk->result = vk->GetQueryPoolResults(
            vk->dev, stopwatch->query->pool, 0, stopwatch->query_count,
            sizeof(*stopwatch->ts) * stopwatch->query_count, stopwatch->ts,
            sizeof(*stopwatch->ts), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        vk_check(vk, "failed to get stopwatch results");
    }

    if (idx + 1 >= stopwatch->query_count)
        vk_die("bad idx");

    const uint64_t cycles = stopwatch->ts[idx + 1] - stopwatch->ts[idx];
    return cycles * (uint64_t)vk->props.properties.limits.timestampPeriod;
}

static inline VkCommandBuffer
vk_begin_cmd(struct vk *vk, bool prot)
{
    VkCommandBuffer *cmd = &vk->submit.cmds[vk->submit.next];
    const uint64_t *sem_val = &vk->submit.sem_vals[vk->submit.next];
    bool *protected_submit = &vk->submit.protected_submits[vk->submit.next];

    const VkSemaphoreWaitInfo wait_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1,
        .pSemaphores = &vk->submit.sem,
        .pValues = sem_val,
    };
    vk->result = vk->WaitSemaphores(vk->dev, &wait_info, UINT64_MAX);
    vk_check(vk, "failed to wait submit semaphore");

    /* reuse or allocate */
    if (*cmd && *protected_submit == prot) {
        vk->result = vk->ResetCommandBuffer(*cmd, 0);
        vk_check(vk, "failed to reset command buffer");
    } else {
        if (*cmd) {
            vk->FreeCommandBuffers(
                vk->dev, *protected_submit ? vk->protected_cmd_pool : vk->cmd_pool, 1, cmd);
        }

        const VkCommandBufferAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = prot ? vk->protected_cmd_pool : vk->cmd_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        vk->result = vk->AllocateCommandBuffers(vk->dev, &alloc_info, cmd);
        vk_check(vk, "failed to allocate command buffer");

        *protected_submit = prot;
    }

    const VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    vk->result = vk->BeginCommandBuffer(*cmd, &begin_info);
    vk_check(vk, "failed to begin command buffer");

    return *cmd;
}

static inline void
vk_end_cmd(struct vk *vk)
{
    VkCommandBuffer cmd = vk->submit.cmds[vk->submit.next];
    uint64_t *sem_val = &vk->submit.sem_vals[vk->submit.next];
    bool protected_submit = vk->submit.protected_submits[vk->submit.next];

    *sem_val = vk->submit.sem_next++;

    /* increment */
    vk->submit.next = (vk->submit.next + 1) % vk->submit.count;

    vk->result = vk->EndCommandBuffer(cmd);
    vk_check(vk, "failed to end command buffer");

    const VkCommandBufferSubmitInfo cmd_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmd,
    };
    const VkSemaphoreSubmitInfo signal_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = vk->submit.sem,
        .value = *sem_val,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    };
    const VkSubmitInfo2 submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .flags = protected_submit ? (VkSubmitFlags)VK_SUBMIT_PROTECTED_BIT : 0,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmd_info,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signal_info,
    };
    vk->result = vk->QueueSubmit2(vk->queue, 1, &submit_info, VK_NULL_HANDLE);
    vk_check(vk, "failed to submit command buffer");
}

static inline void
vk_wait(struct vk *vk)
{
    vk->result = vk->QueueWaitIdle(vk->queue);
    vk_check(vk, "failed to wait queue");
}

static inline void
vk_validate_swapchain(struct vk *vk, const struct vk_swapchain *swapchain)
{
    if (!vk->KHR_swapchain)
        vk_die("VK_KHR_swapchain is disabled");

    /* check support */
    VkBool32 supported;
    vk->result = vk->GetPhysicalDeviceSurfaceSupportKHR(vk->physical_dev, vk->queue_family_index,
                                                        swapchain->info.surface, &supported);
    vk_check(vk, "failed to get surface support");
    if (!supported)
        vk_die("surface is unsupported");

    /* check caps */
    VkSurfaceCapabilitiesKHR caps;
    vk->result = vk->GetPhysicalDeviceSurfaceCapabilitiesKHR(vk->physical_dev,
                                                             swapchain->info.surface, &caps);
    vk_check(vk, "failed to get surface caps");
    if (swapchain->info.imageExtent.width < caps.minImageExtent.width ||
        swapchain->info.imageExtent.width > caps.maxImageExtent.width ||
        swapchain->info.imageExtent.height < caps.minImageExtent.height ||
        swapchain->info.imageExtent.height > caps.maxImageExtent.height) {
        vk_die("bad swapchain extent: req %dx%d min %dx%d max %dx%d",
               swapchain->info.imageExtent.width, swapchain->info.imageExtent.height,
               caps.minImageExtent.width, caps.minImageExtent.height, caps.maxImageExtent.width,
               caps.maxImageExtent.height);
    }

    if (swapchain->info.minImageCount < caps.minImageCount ||
        swapchain->info.minImageCount < caps.maxImageCount)
        vk_die("swapchain min image count %d is invalid", swapchain->info.minImageCount);

    /* check format */
    VkSurfaceFormatKHR fmts[8];
    uint32_t count = ARRAY_SIZE(fmts);
    vk->result = vk->GetPhysicalDeviceSurfaceFormatsKHR(vk->physical_dev, swapchain->info.surface,
                                                        &count, fmts);
    vk_check(vk, "failed to get surface formats");

    bool found = false;
    for (uint32_t i = 0; i < count; i++) {
        if (fmts[i].format == swapchain->info.imageFormat &&
            fmts[i].colorSpace == swapchain->info.imageColorSpace) {
            found = true;
            break;
        }
    }
    if (!found)
        vk_die("%d is an invalid format", swapchain->info.imageFormat);

    /* check present mode */
    VkPresentModeKHR modes[8];
    count = ARRAY_SIZE(modes);
    vk->result = vk->GetPhysicalDeviceSurfacePresentModesKHR(
        vk->physical_dev, swapchain->info.surface, &count, modes);
    vk_check(vk, "failed to get surface present modes");

    found = false;
    for (uint32_t i = 0; i < count; i++) {
        if (modes[i] == swapchain->info.presentMode) {
            found = true;
            break;
        }
    }
    if (!found)
        vk_die("%d is invalid present mode", swapchain->info.presentMode);
}

static inline void
vk_recreate_swapchain(struct vk *vk,
                      struct vk_swapchain *swapchain,
                      uint32_t width,
                      uint32_t height)
{
    swapchain->info.imageExtent = (VkExtent2D){
        .width = width,
        .height = height,
    };
    swapchain->info.oldSwapchain = swapchain->swapchain;

    vk_validate_swapchain(vk, swapchain);

    vk->result = vk->CreateSwapchainKHR(vk->dev, &swapchain->info, NULL, &swapchain->swapchain);
    vk_check(vk, "failed to create swapchain");

    if (swapchain->info.oldSwapchain != VK_NULL_HANDLE) {
        vk->DestroySwapchainKHR(vk->dev, swapchain->info.oldSwapchain, NULL);
        free(swapchain->img_handles);
        free(swapchain->imgs);
    }

    vk->result =
        vk->GetSwapchainImagesKHR(vk->dev, swapchain->swapchain, &swapchain->img_count, NULL);
    vk_check(vk, "failed to get swapchain image count");

    swapchain->img_handles =
        (VkImage *)calloc(swapchain->img_count, sizeof(*swapchain->img_handles));
    swapchain->imgs = (struct vk_image *)calloc(swapchain->img_count, sizeof(*swapchain->imgs));
    if (!swapchain->img_handles || !swapchain->imgs)
        vk_die("failed to alloc swapchain imgs");

    vk->result = vk->GetSwapchainImagesKHR(vk->dev, swapchain->swapchain, &swapchain->img_count,
                                           swapchain->img_handles);
    vk_check(vk, "failed to get swapchain images");

    for (uint32_t i = 0; i < swapchain->img_count; i++) {
        struct vk_image *img = &swapchain->imgs[i];

        img->info = (VkImageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = swapchain->info.imageFormat,
            .extent = {
                .width = swapchain->info.imageExtent.width,
                .height = swapchain->info.imageExtent.height,
                .depth = 1,
            },
            .mipLevels = 1,
            .arrayLayers = swapchain->info.imageArrayLayers,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = swapchain->info.imageUsage,
            .sharingMode = swapchain->info.imageSharingMode,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        img->features = vk_validate_image_info(vk, &img->info);

        img->img = swapchain->img_handles[i];
    }
}

static inline struct vk_swapchain *
vk_create_swapchain(struct vk *vk,
                    VkSwapchainCreateFlagsKHR flags,
                    VkSurfaceKHR surf,
                    VkFormat format,
                    uint32_t width,
                    uint32_t height,
                    VkPresentModeKHR mode,
                    VkImageUsageFlags usage)
{
    VkSurfaceCapabilitiesKHR surf_caps;
    vk->result = vk->GetPhysicalDeviceSurfaceCapabilitiesKHR(vk->physical_dev, surf, &surf_caps);
    vk_check(vk, "failed to get surface caps");

    struct vk_swapchain *swapchain = (struct vk_swapchain *)calloc(1, sizeof(*swapchain));
    if (!swapchain)
        vk_die("failed to alloc swapchain");

    swapchain->info = (VkSwapchainCreateInfoKHR){
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .flags = flags,
        .surface = surf,
        .minImageCount = surf_caps.minImageCount,
        .imageFormat = format,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = {
            .width = width,
            .height = height,
        },
        .imageArrayLayers = 1,
        .imageUsage = usage,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = mode,
        .clipped = true,
        .oldSwapchain = VK_NULL_HANDLE,
    };
    vk_recreate_swapchain(vk, swapchain, width, height);

    const VkFenceCreateInfo fence_create_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    vk->result = vk->CreateFence(vk->dev, &fence_create_info, NULL, &swapchain->fence);
    vk_check(vk, "failed to create swapchain fence");

    return swapchain;
}

static inline struct vk_image *
vk_acquire_swapchain_image(struct vk *vk, struct vk_swapchain *swapchain)
{
    const VkAcquireNextImageInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
        .swapchain = swapchain->swapchain,
        .timeout = UINT64_MAX,
        .fence = swapchain->fence,
        .deviceMask = 0x1,
    };
    vk->result = vk->AcquireNextImage2KHR(vk->dev, &info, &swapchain->img_cur);

    switch (vk->result) {
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
        vk->result = vk->WaitForFences(vk->dev, 1, &swapchain->fence, true, UINT64_MAX);
        vk_check(vk, "failed to wait for swapchain img");
        vk->result = vk->ResetFences(vk->dev, 1, &swapchain->fence);
        vk_check(vk, "failed to reset for swapchain img");
        return &swapchain->imgs[swapchain->img_cur];
    case VK_ERROR_OUT_OF_DATE_KHR:
        return NULL;
    default:
        vk_die("failed to acquire swapchain img");
    }
}

static inline VkResult
vk_present_swapchain_image(struct vk *vk, struct vk_swapchain *swapchain)
{
    const VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount = 1,
        .pSwapchains = &swapchain->swapchain,
        .pImageIndices = &swapchain->img_cur,
    };
    vk->result = vk->QueuePresentKHR(vk->queue, &present_info);

    switch (vk->result) {
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
    case VK_ERROR_OUT_OF_DATE_KHR:
        return vk->result;
    default:
        vk_die("failed to present swapchain img");
    }
}

static inline void
vk_destroy_swapchain(struct vk *vk, struct vk_swapchain *swapchain)
{
    vk->DestroyFence(vk->dev, swapchain->fence, NULL);
    vk->DestroySwapchainKHR(vk->dev, swapchain->swapchain, NULL);

    free(swapchain->img_handles);
    free(swapchain->imgs);
    free(swapchain);
}

#endif /* VKUTIL_H */
