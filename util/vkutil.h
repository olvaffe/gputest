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

#define VKUTIL_MIN_API_VERSION VK_API_VERSION_1_1

struct vk_init_params {
    const char *render_node;

    uint32_t api_version;
    bool enable_all_features;
    bool protected_memory;

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

    VkPhysicalDeviceDrmPropertiesEXT drm_props;

    VkPhysicalDeviceFeatures2 features;
    VkPhysicalDeviceVulkan11Features vulkan_11_features;
    VkPhysicalDeviceVulkan12Features vulkan_12_features;
    VkPhysicalDeviceVulkan13Features vulkan_13_features;

    VkPhysicalDeviceSamplerYcbcrConversionFeatures sampler_ycbcr_conversion_features;
    VkPhysicalDeviceHostQueryResetFeatures host_query_reset_features;
    VkPhysicalDeviceCustomBorderColorFeaturesEXT custom_border_color_features;
    VkPhysicalDeviceProtectedMemoryFeatures protected_memory_features;

    VkPhysicalDeviceMemoryProperties mem_props;
    uint32_t buf_mt_index;

    VkDevice dev;
    VkQueue queue;
    uint32_t queue_family_index;

    VkDescriptorPool desc_pool;

    VkCommandPool cmd_pool;
    VkCommandPool protected_cmd_pool;
    struct {
        VkCommandBuffer cmds[4];
        VkFence fences[4];
        bool protected_submits[4];
        uint32_t count;
        uint32_t next;
    } submit;
};

struct vk_buffer {
    VkBufferCreateInfo info;
    VkBuffer buf;

    VkDeviceMemory mem;
    VkDeviceSize mem_size;
    void *mem_ptr;
};

struct vk_image {
    VkImageCreateInfo info;
    VkFormatFeatureFlags features;
    VkImage img;

    VkDeviceMemory mem;
    VkDeviceSize mem_size;
    bool mem_mappable;

    VkImageView render_view;

    VkSamplerYcbcrConversion ycbcr_conv;
    uint32_t ycbcr_conv_desc_count;

    VkImageView sample_view;
    VkImageViewType sample_view_type;
    VkSampler sampler;
};

struct vk_framebuffer {
    VkRenderPass pass;
    VkFramebuffer fb;

    uint32_t width;
    uint32_t height;
    VkSampleCountFlagBits samples;
};

struct vk_pipeline {
    VkPipelineShaderStageCreateInfo stages[5];
    uint32_t stage_count;

    /* vertex input state */
    VkVertexInputBindingDescription vi_binding;
    VkVertexInputAttributeDescription vi_attrs[16];
    uint32_t vi_attr_count;
    VkPipelineInputAssemblyStateCreateInfo ia_info;

    /* pre-rasterization shader state */
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineRasterizationStateCreateInfo rast_info;
    VkPipelineTessellationStateCreateInfo tess_info;

    /* fragment shader state */
    VkPipelineMultisampleStateCreateInfo msaa_info;
    VkSampleMask sample_mask;
    VkPipelineDepthStencilStateCreateInfo depth_info;

    /* fragment output state */
    VkPipelineColorBlendAttachmentState color_att;
    VkPipelineRenderingCreateInfo rendering_info;
    const struct vk_framebuffer *fb;

    VkDescriptorSetLayout set_layouts[4];
    uint32_t set_layout_count;
    VkPushConstantRange push_const;
    VkPipelineLayout pipeline_layout;

    VkPipeline pipeline;
};

struct vk_descriptor_set {
    VkDescriptorSet set;
};

struct vk_semaphore {
    VkSemaphore sem;
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

static inline void PRINTFLIKE(1, 2) vk_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_logv("VK", format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(1, 2) NORETURN vk_die(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_diev("VK", format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(2, 3) vk_check(const struct vk *vk, const char *format, ...)
{
    if (vk->result == VK_SUCCESS)
        return;

    va_list ap;
    va_start(ap, format);
    if (vk->result > VK_SUCCESS)
        u_logv("VK", format, ap);
    else
        u_diev("VK", format, ap);
    va_end(ap);
}

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
    vk->GetPhysicalDeviceMemoryProperties(vk->physical_dev, &vk->mem_props);

    const VkMemoryPropertyFlags mt_flags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    bool mt_found = false;
    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        const VkMemoryType *mt = &vk->mem_props.memoryTypes[i];
        if ((mt->propertyFlags & mt_flags) == mt_flags) {
            vk->buf_mt_index = i;
            mt_found = true;
            break;
        }
    }
    if (!mt_found)
        vk_die("failed to find a coherent and visible memory type for buffers");
}

static inline void
vk_init_physical_device_features(struct vk *vk)
{
    vk->features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    void **pnext = &vk->features.pNext;
    if (vk->params.api_version >= VK_API_VERSION_1_2) {
        vk->vulkan_11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        vk->vulkan_12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

        *pnext = &vk->vulkan_11_features;
        vk->vulkan_11_features.pNext = &vk->vulkan_12_features;
        pnext = &vk->vulkan_12_features.pNext;
    } else {
        vk->sampler_ycbcr_conversion_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;
        vk->host_query_reset_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES;

        *pnext = &vk->sampler_ycbcr_conversion_features;
        vk->sampler_ycbcr_conversion_features.pNext = &vk->host_query_reset_features;
        pnext = &vk->host_query_reset_features.pNext;
    }
    if (vk->params.api_version >= VK_API_VERSION_1_3) {
        vk->vulkan_13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        *pnext = &vk->vulkan_13_features;
        pnext = &vk->vulkan_13_features.pNext;
    }

    vk->custom_border_color_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT;
    *pnext = &vk->custom_border_color_features;
    pnext = &vk->custom_border_color_features.pNext;

    if (vk->params.protected_memory) {
        vk->protected_memory_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES;
        *pnext = &vk->protected_memory_features;
        pnext = &vk->protected_memory_features.pNext;
    }

    vk->GetPhysicalDeviceFeatures2(vk->physical_dev, &vk->features);
}

static inline void
vk_init_physical_device_properties(struct vk *vk)
{
    vk->props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

    void **pnext = &vk->props.pNext;
    if (vk->params.api_version >= VK_API_VERSION_1_2) {
        vk->vulkan_11_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
        vk->vulkan_12_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;

        *pnext = &vk->vulkan_11_props;
        vk->vulkan_11_props.pNext = &vk->vulkan_12_props;
        pnext = &vk->vulkan_12_props.pNext;
    }
    if (vk->params.api_version >= VK_API_VERSION_1_3) {
        vk->vulkan_13_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;

        *pnext = &vk->vulkan_13_props;
        pnext = &vk->vulkan_13_props.pNext;
    }

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
vk_init_device_enabled_features(struct vk *vk, VkPhysicalDeviceFeatures2 *features)
{
    /* check minimum features */
    if (!vk->features.features.tessellationShader)
        vk_die("no tessellation shader support");
    if (!vk->features.features.geometryShader)
        vk_die("no geometry shader support");
    if (!vk->features.features.fillModeNonSolid)
        vk_die("no non-solid fill mode support");
    if (vk->params.api_version >= VK_API_VERSION_1_2) {
        if (vk->params.protected_memory && !vk->vulkan_11_features.protectedMemory)
            vk_die("no protected memory support");

        // if (!vk->vulkan_11_features.samplerYcbcrConversion)
        //     vk_die("no ycbcr conversion support");

        if (!vk->vulkan_12_features.hostQueryReset)
            vk_die("no host query reset support");
    } else {
        if (vk->params.protected_memory && !vk->protected_memory_features.protectedMemory)
            vk_die("no protected memory support");

        // if (!vk->sampler_ycbcr_conversion_features.samplerYcbcrConversion)
        //     vk_die("no ycbcr conversion support");
    }

    if (vk->params.enable_all_features) {
        *features = vk->features;
        return;
    }

    *features = (VkPhysicalDeviceFeatures2){
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .features = {
            .geometryShader = true,
            .tessellationShader = true,
            .fillModeNonSolid = true,
        },
    };

    void **pnext = &features->pNext;
    if (vk->params.api_version >= VK_API_VERSION_1_2) {
        *pnext = &vk->vulkan_11_features;
        vk->vulkan_11_features.pNext = &vk->vulkan_12_features;
        pnext = &vk->vulkan_12_features.pNext;
    } else {
        *pnext = &vk->sampler_ycbcr_conversion_features;
        pnext = &vk->sampler_ycbcr_conversion_features.pNext;
    }
    if (vk->params.api_version >= VK_API_VERSION_1_3) {
        *pnext = &vk->vulkan_13_features;
        pnext = &vk->vulkan_13_features.pNext;
    }
    if (vk->EXT_custom_border_color) {
        *pnext = &vk->custom_border_color_features;
        pnext = &vk->custom_border_color_features.pNext;
    }
    if (vk->params.protected_memory) {
        *pnext = &vk->protected_memory_features;
        pnext = &vk->protected_memory_features.pNext;
    }

    *pnext = NULL;
}

static inline void
vk_init_device(struct vk *vk)
{
    VkPhysicalDeviceFeatures2 enabled_features;
    vk_init_device_enabled_features(vk, &enabled_features);

    vk->queue_family_index = 0;

    VkQueueFamilyProperties queue_props;
    uint32_t queue_count = 1;
    vk->GetPhysicalDeviceQueueFamilyProperties(vk->physical_dev, &queue_count, &queue_props);
    if (!(queue_props.queueFlags & VK_QUEUE_GRAPHICS_BIT))
        vk_die("queue family 0 does not support graphics");
    if (vk->params.protected_memory && !(queue_props.queueFlags & VK_QUEUE_PROTECTED_BIT))
        vk_die("queue family 0 does not support protected");
    if (!queue_props.timestampValidBits)
        vk_die("queue family 0 does not support timestamps");

    const VkDeviceQueueCreateFlags queue_flags =
        vk->params.protected_memory ? VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT : 0;
    const float queue_priority = 1.0f;
    const VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .flags = queue_flags,
        .queueFamilyIndex = vk->queue_family_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };
    const VkDeviceCreateInfo dev_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &enabled_features,
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
            .descriptorCount = 256,
        },
        [1] = {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 256,
        },
        [2] = {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 256,
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

    static_assert(ARRAY_SIZE(vk->submit.cmds) == ARRAY_SIZE(vk->submit.fences), "");
    static_assert(ARRAY_SIZE(vk->submit.cmds) == ARRAY_SIZE(vk->submit.protected_submits), "");
    vk->submit.count = ARRAY_SIZE(vk->submit.cmds);

    /* avoid accessing dangling pointers */
    vk->params.instance_ext_count = 0;
    vk->params.dev_ext_count = 0;
}

static inline void
vk_cleanup(struct vk *vk)
{
    vk->DeviceWaitIdle(vk->dev);

    for (uint32_t i = 0; i < vk->submit.count; i++) {
        if (vk->submit.fences[i] == VK_NULL_HANDLE)
            break;
        vk->DestroyFence(vk->dev, vk->submit.fences[i], NULL);
    }

    vk->DestroyDescriptorPool(vk->dev, vk->desc_pool, NULL);
    vk->DestroyCommandPool(vk->dev, vk->cmd_pool, NULL);

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
                      VkBufferUsageFlags usage)
{
    const struct VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .flags = flags,
        .size = size,
        .usage = usage,
    };
    VkBuffer buf;
    vk->result = vk->CreateBuffer(vk->dev, &info, NULL, &buf);
    vk_check(vk, "failed to create test buffer");

    VkMemoryRequirements reqs;
    vk->GetBufferMemoryRequirements(vk->dev, buf, &reqs);

    vk->DestroyBuffer(vk->dev, buf, NULL);

    return reqs.memoryTypeBits;
}

static inline struct vk_buffer *
vk_create_buffer_with_mt(struct vk *vk,
                         VkBufferCreateFlags flags,
                         VkDeviceSize size,
                         VkBufferUsageFlags usage,
                         uint32_t mt_idx)
{
    struct vk_buffer *buf = (struct vk_buffer *)calloc(1, sizeof(*buf));
    if (!buf)
        vk_die("failed to alloc buf");

    buf->info = (VkBufferCreateInfo){
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .flags = flags,
        .size = size,
        .usage = usage,
    };

    vk->result = vk->CreateBuffer(vk->dev, &buf->info, NULL, &buf->buf);
    vk_check(vk, "failed to create buffer");

    VkMemoryRequirements reqs;
    vk->GetBufferMemoryRequirements(vk->dev, buf->buf, &reqs);
    if (!(reqs.memoryTypeBits & (1u << mt_idx)))
        vk_die("failed to meet buf memory reqs: 0x%x", reqs.memoryTypeBits);

    buf->mem = vk_alloc_memory(vk, reqs.size, mt_idx);
    buf->mem_size = reqs.size;

    const VkMemoryType *mt = &vk->mem_props.memoryTypes[mt_idx];
    if (mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        vk->result = vk->MapMemory(vk->dev, buf->mem, 0, buf->mem_size, 0, &buf->mem_ptr);
        vk_check(vk, "failed to map buffer memory");
    }

    vk->result = vk->BindBufferMemory(vk->dev, buf->buf, buf->mem, 0);
    vk_check(vk, "failed to bind buffer memory");

    return buf;
}

static inline struct vk_buffer *
vk_create_buffer(struct vk *vk,
                 VkBufferCreateFlags flags,
                 VkDeviceSize size,
                 VkBufferUsageFlags usage)
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

static inline void
vk_validate_image(struct vk *vk, struct vk_image *img)
{
    const struct {
        VkImageUsageFlagBits usage;
        VkFormatFeatureFlagBits feature;
    } pairs[] = {
        { VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_FORMAT_FEATURE_TRANSFER_SRC_BIT },
        { VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_FORMAT_FEATURE_TRANSFER_DST_BIT },
        { VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT },
        { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT },
        { VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
          VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT },
    };

    for (uint32_t i = 0; i < ARRAY_SIZE(pairs); i++) {
        if ((img->info.usage & pairs[i].usage) && !(img->features & pairs[i].feature))
            vk_die("image usage 0x%x is not supported", img->info.usage);
    }

    VkImageFormatProperties img_props;
    vk->result = vk->GetPhysicalDeviceImageFormatProperties(
        vk->physical_dev, img->info.format, img->info.imageType, img->info.tiling,
        img->info.usage, img->info.flags, &img_props);
    vk_check(vk, "image format/type/tiling/usage/flags is not supported");

    if (img->info.extent.width > img_props.maxExtent.width)
        vk_die("image width %u is not supported", img->info.extent.width);
    if (img->info.extent.height > img_props.maxExtent.height)
        vk_die("image height %u is not supported", img->info.extent.height);
    if (img->info.extent.depth > img_props.maxExtent.depth)
        vk_die("image depth %u is not supported", img->info.extent.depth);
    if (img->info.mipLevels > img_props.maxMipLevels)
        vk_die("image miplevel %u is not supported", img->info.mipLevels);
    if (img->info.arrayLayers > img_props.maxArrayLayers)
        vk_die("image array layer %u is not supported", img->info.arrayLayers);
    if (!(img->info.samples & img_props.sampleCounts))
        vk_die("image sample count %u is not supported", img->info.samples);
}

static inline void
vk_init_image(struct vk *vk, struct vk_image *img)
{
    VkFormatProperties2 fmt_props = {
        .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
    };
    vk->GetPhysicalDeviceFormatProperties2(vk->physical_dev, img->info.format, &fmt_props);
    img->features = img->info.tiling == VK_IMAGE_TILING_OPTIMAL
                        ? fmt_props.formatProperties.optimalTilingFeatures
                        : fmt_props.formatProperties.linearTilingFeatures;

    vk_validate_image(vk, img);

    vk->result = vk->CreateImage(vk->dev, &img->info, NULL, &img->img);
    vk_check(vk, "failed to create image");

    VkMemoryRequirements reqs;
    vk->GetImageMemoryRequirements(vk->dev, img->img, &reqs);

    uint32_t mt_index;
    if (reqs.memoryTypeBits & (1u << vk->buf_mt_index)) {
        mt_index = vk->buf_mt_index;
        img->mem_mappable = true;
    } else {
        mt_index = ffs(reqs.memoryTypeBits) - 1;
        img->mem_mappable = false;
    }

    img->mem = vk_alloc_memory(vk, reqs.size, mt_index);
    img->mem_size = reqs.size;

    vk->result = vk->BindImageMemory(vk->dev, img->img, img->mem, 0);
    vk_check(vk, "failed to bind image memory");
}

static inline struct vk_image *
vk_create_image_from_info(struct vk *vk, const VkImageCreateInfo *info)
{
    struct vk_image *img = (struct vk_image *)calloc(1, sizeof(*img));
    if (!img)
        vk_die("failed to alloc img");

    img->info = *info;
    vk_init_image(vk, img);

    return img;
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

    vk_init_image(vk, img);

    void *ptr;
    vk->result = vk->MapMemory(vk->dev, img->mem, 0, img->mem_size, 0, &ptr);
    vk_check(vk, "failed to map image");

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
        const VkImageSubresource y_subres = {
            .aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT,
        };
        const VkImageSubresource uv_subres = {
            .aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT,
        };
        VkSubresourceLayout y_layout;
        VkSubresourceLayout uv_layout;
        vk->GetImageSubresourceLayout(vk->dev, img->img, &y_subres, &y_layout);
        vk->GetImageSubresourceLayout(vk->dev, img->img, &uv_subres, &uv_layout);

        conv.dst_plane_ptrs[0] = ptr + y_layout.offset;
        conv.dst_plane_strides[0] = y_layout.rowPitch;
        conv.dst_plane_ptrs[1] = ptr + uv_layout.offset;
        conv.dst_plane_strides[1] = uv_layout.rowPitch;
    } else {
        const VkImageSubresource subres = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        };
        VkSubresourceLayout layout;
        vk->GetImageSubresourceLayout(vk->dev, img->img, &subres, &layout);

        conv.dst_plane_ptrs[0] = ptr + layout.offset;
        conv.dst_plane_strides[0] = layout.rowPitch;
    }

    u_convert_format(&conv);

    vk->UnmapMemory(vk->dev, img->mem);

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
        !(img->features & VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT))
        vk_die("image does not support midpoint chroma offset");
    else if (chroma_offset == VK_CHROMA_LOCATION_COSITED_EVEN &&
             !(img->features & VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT))
        vk_die("image does not support cosited chroma offset");

    if (chroma_filter == VK_FILTER_LINEAR &&
        !(img->features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT))
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

    VkClearColorValue custom_border_color = { 0 };
    VkBorderColor border_color;
    if (vk->EXT_custom_border_color) {
        custom_border_color = (VkClearColorValue){
            .uint32 = { 10, 0, 0, 0 },
        };
        border_color = VK_BORDER_COLOR_INT_CUSTOM_EXT;
    } else {
        border_color = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    }
    const VkSamplerCustomBorderColorCreateInfoEXT border_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT,
        .customBorderColor = custom_border_color,
        .format = img->info.format,
    };

    const VkSamplerAddressMode addr_mode = img->ycbcr_conv != VK_NULL_HANDLE
                                               ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
                                               : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    const VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = img->ycbcr_conv != VK_NULL_HANDLE ? (void *)&conv_info : (void *)&border_info,
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
    if (!img->mem_mappable)
        vk_die("cannot fill non-mappable image");

    if (img->info.tiling != VK_IMAGE_TILING_LINEAR)
        vk_log("filling non-linear image");

    void *ptr;
    vk->result = vk->MapMemory(vk->dev, img->mem, 0, img->mem_size, 0, &ptr);
    vk_check(vk, "failed to map image");
    memset(ptr, val, img->mem_size);
    vk->UnmapMemory(vk->dev, img->mem);
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
                const uint32_t *pixel = (const uint32_t *)(data + pitch * y + cpp * x);
                /* discard the higher bytes */
                const char bytes[3] = { (char)pixel[swizzle[0]], (char)pixel[swizzle[1]],
                                        (char)pixel[swizzle[2]] };
                if (fwrite(bytes, sizeof(bytes), 1, fp) != 1)
                    vk_die("failed to write pixel (%u, %u)", x, y);
            } else if (packed) {
                const uint16_t *pixel = (const uint16_t *)(data + pitch * y + cpp * x);
                uint16_t val = *pixel;
                if (format == VK_FORMAT_R5G5B5A1_UNORM_PACK16)
                    val >>= 1;

                const char comps[3] = { (char)(val & 0x1f), (char)((val >> 5) & 0x1f),
                                        (char)((val >> 10) & 0x1f) };
                const char bytes[3] = { comps[swizzle[0]], comps[swizzle[1]], comps[swizzle[2]] };
                if (fwrite(bytes, sizeof(bytes), 1, fp) != 1)
                    vk_die("failed to write pixel (%u, %u)", x, y);
            } else {
                const char *pixel = (const char *)(data + pitch * y + cpp * x);
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
    if (!img->mem_mappable)
        vk_die("cannot dump non-mappable image");

    if (img->info.tiling != VK_IMAGE_TILING_LINEAR)
        vk_log("dumping non-linear image");

    if (img->info.samples != VK_SAMPLE_COUNT_1_BIT)
        vk_log("dumping msaa image");

    const VkImageSubresource subres = {
        .aspectMask = aspect,
    };
    VkSubresourceLayout layout;
    vk->GetImageSubresourceLayout(vk->dev, img->img, &subres, &layout);

    void *ptr;
    vk->result = vk->MapMemory(vk->dev, img->mem, 0, img->mem_size, 0, &ptr);
    vk_check(vk, "failed to map image memory");

    vk_write_ppm(filename, ptr + layout.offset, img->info.format,
                 img->info.extent.width * img->info.samples, img->info.extent.height,
                 layout.rowPitch);

    vk->UnmapMemory(vk->dev, img->mem);
}

static inline void
vk_dump_image_raw(struct vk *vk, struct vk_image *img, const char *filename)
{
    if (!img->mem_mappable)
        vk_die("cannot dump non-mappable image");

    void *ptr;
    vk->result = vk->MapMemory(vk->dev, img->mem, 0, img->mem_size, 0, &ptr);
    vk_check(vk, "failed to map image memory");

    FILE *fp = fopen(filename, "w");
    if (!fp)
        vk_die("failed to open %s", filename);
    if (fwrite(ptr, 1, img->mem_size, fp) != img->mem_size)
        vk_die("failed to write raw memory");
    fclose(fp);

    vk->UnmapMemory(vk->dev, img->mem);
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
    if (fwrite(buf->mem_ptr + offset, 1, size, fp) != size)
        vk_die("failed to write raw memory");
    fclose(fp);
}

static inline struct vk_framebuffer *
vk_create_framebuffer(struct vk *vk,
                      struct vk_image *color,
                      struct vk_image *resolve,
                      struct vk_image *depth,
                      VkAttachmentLoadOp load_op,
                      VkAttachmentStoreOp store_op)
{
    struct vk_framebuffer *fb = (struct vk_framebuffer *)calloc(1, sizeof(*fb));
    if (!fb)
        vk_die("failed to alloc fb");

    VkAttachmentReference color_ref = { .attachment = VK_ATTACHMENT_UNUSED };
    VkAttachmentReference resolve_ref = { .attachment = VK_ATTACHMENT_UNUSED };
    VkAttachmentReference depth_ref = { .attachment = VK_ATTACHMENT_UNUSED };
    VkAttachmentDescription att_descs[3];
    VkImageView views[3];
    uint32_t att_count = 0;

    if (color) {
        att_descs[att_count] = (VkAttachmentDescription){
            .format = color->info.format,
            .samples = color->info.samples,
            .loadOp = load_op,
            .storeOp = store_op,
            .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        color_ref = (VkAttachmentReference){
            .attachment = att_count,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        views[att_count] = color->render_view;
        att_count++;
    }

    if (resolve) {
        att_descs[att_count] = (VkAttachmentDescription){
            .format = resolve->info.format,
            .samples = resolve->info.samples,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = store_op,
            .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        resolve_ref = (VkAttachmentReference){
            .attachment = att_count,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        views[att_count] = resolve->render_view;
        att_count++;
    }

    if (depth) {
        att_descs[att_count] = (VkAttachmentDescription){
            .format = depth->info.format,
            .samples = depth->info.samples,
            .loadOp = load_op,
            .storeOp = store_op,
            .stencilLoadOp = load_op,
            .stencilStoreOp = store_op,
            .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        depth_ref = (VkAttachmentReference){
            .attachment = att_count,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        views[att_count] = depth->render_view;
        att_count++;
    }

    const VkSubpassDescription subpass_desc = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = (uint32_t)(color ? 1 : 0),
        .pColorAttachments = color ? &color_ref : NULL,
        .pResolveAttachments = resolve ? &resolve_ref : NULL,
        .pDepthStencilAttachment = depth ? &depth_ref : NULL,
    };
    const VkRenderPassCreateInfo pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = att_count,
        .pAttachments = att_descs,
        .subpassCount = 1,
        .pSubpasses = &subpass_desc,
    };

    vk->result = vk->CreateRenderPass(vk->dev, &pass_info, NULL, &fb->pass);
    vk_check(vk, "failed to create render pass");

    const VkFramebufferCreateInfo fb_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = fb->pass,
        .attachmentCount = att_count,
        .pAttachments = views,
        .width = color ? color->info.extent.width : depth->info.extent.width,
        .height = color ? color->info.extent.height : depth->info.extent.height,
        .layers = color ? color->info.arrayLayers : depth->info.arrayLayers,
    };

    vk->result = vk->CreateFramebuffer(vk->dev, &fb_info, NULL, &fb->fb);
    vk_check(vk, "failed to create framebuffer");

    fb->width = fb_info.width;
    fb->height = fb_info.height;
    fb->samples = color ? color->info.samples : depth->info.samples;

    return fb;
}

static inline void
vk_destroy_framebuffer(struct vk *vk, struct vk_framebuffer *fb)
{
    vk->DestroyRenderPass(vk->dev, fb->pass, NULL);
    vk->DestroyFramebuffer(vk->dev, fb->fb, NULL);
    free(fb);
}

static inline struct vk_pipeline *
vk_create_pipeline(struct vk *vk)
{
    struct vk_pipeline *pipeline = (struct vk_pipeline *)calloc(1, sizeof(*pipeline));
    if (!pipeline)
        vk_die("failed to alloc pipeline");

    return pipeline;
}

static inline VkShaderModule
vk_create_shader_module(struct vk *vk, const uint32_t *code, size_t size)
{
    const VkShaderModuleCreateInfo mod_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = code,
    };

    VkShaderModule mod;
    vk->result = vk->CreateShaderModule(vk->dev, &mod_info, NULL, &mod);
    vk_check(vk, "failed to create shader module");

    return mod;
}

static inline void
vk_add_pipeline_shader(struct vk *vk,
                       struct vk_pipeline *pipeline,
                       VkShaderStageFlagBits stage,
                       const uint32_t *code,
                       size_t size)
{
    pipeline->stages[pipeline->stage_count++] = (VkPipelineShaderStageCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = stage,
        .module = vk_create_shader_module(vk, code, size),
        .pName = "main",
    };
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
vk_set_pipeline_topology(struct vk *vk,
                         struct vk_pipeline *pipeline,
                         VkPrimitiveTopology topology)
{
    pipeline->ia_info = (VkPipelineInputAssemblyStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = topology,
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
vk_set_pipeline_rasterization(struct vk *vk,
                              struct vk_pipeline *pipeline,
                              VkPolygonMode poly_mode)
{
    pipeline->rast_info = (VkPipelineRasterizationStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = poly_mode,
        .lineWidth = 1.0f,
    };
}

static inline void
vk_set_pipeline_tessellation(struct vk *vk, struct vk_pipeline *pipeline, uint32_t cp_count)
{
    pipeline->tess_info = (VkPipelineTessellationStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
        .patchControlPoints = cp_count,
    };
}

static inline void
vk_set_pipeline_sample_count(struct vk *vk,
                             struct vk_pipeline *pipeline,
                             VkSampleCountFlagBits sample_count)
{
    pipeline->sample_mask = (1u << sample_count) - 1;
    pipeline->msaa_info = (VkPipelineMultisampleStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = sample_count,
        .pSampleMask = &pipeline->sample_mask,
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
vk_set_pipeline_push_const(struct vk *vk,
                           struct vk_pipeline *pipeline,
                           VkShaderStageFlags stages,
                           uint32_t size)
{
    pipeline->push_const = (VkPushConstantRange){
        .stageFlags = stages,
        .size = size,
    };
}

static inline void
vk_setup_pipeline(struct vk *vk, struct vk_pipeline *pipeline, const struct vk_framebuffer *fb)
{
    const VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = pipeline->set_layout_count,
        .pSetLayouts = pipeline->set_layouts,
        .pushConstantRangeCount = (uint32_t)(pipeline->push_const.size ? 1 : 0),
        .pPushConstantRanges = &pipeline->push_const,
    };
    vk->result = vk->CreatePipelineLayout(vk->dev, &pipeline_layout_info, NULL,
                                          &pipeline->pipeline_layout);
    vk_check(vk, "failed to create pipeline layout");

    pipeline->depth_info = (VkPipelineDepthStencilStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    };

    pipeline->color_att = (VkPipelineColorBlendAttachmentState){
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    pipeline->rendering_info = (VkPipelineRenderingCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
    };

    pipeline->fb = fb;
}

static inline void
vk_compile_pipeline(struct vk *vk, struct vk_pipeline *pipeline)
{
    if (pipeline->stage_count == 1 && pipeline->stages[0].stage == VK_SHADER_STAGE_COMPUTE_BIT) {
        const VkComputePipelineCreateInfo compute_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = pipeline->stages[0],
            .layout = pipeline->pipeline_layout,
        };
        vk->result = vk->CreateComputePipelines(vk->dev, VK_NULL_HANDLE, 1, &compute_info, NULL,
                                                &pipeline->pipeline);
        vk_check(vk, "failed to create compute pipeline");
        return;
    }

    const VkPipelineVertexInputStateCreateInfo vi_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = (uint32_t)(pipeline->vi_attr_count ? 1 : 0),
        .pVertexBindingDescriptions = &pipeline->vi_binding,
        .vertexAttributeDescriptionCount = pipeline->vi_attr_count,
        .pVertexAttributeDescriptions = pipeline->vi_attrs,
    };

    const VkPipelineViewportStateCreateInfo vp_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &pipeline->viewport,
        .scissorCount = 1,
        .pScissors = &pipeline->scissor,
    };

    const VkPipelineColorBlendStateCreateInfo color_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &pipeline->color_att,
    };

    const VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = pipeline->fb ? NULL : &pipeline->rendering_info,
        .stageCount = pipeline->stage_count,
        .pStages = pipeline->stages,
        .pVertexInputState = &vi_info,
        .pInputAssemblyState = &pipeline->ia_info,
        .pTessellationState = &pipeline->tess_info,
        .pViewportState = &vp_info,
        .pRasterizationState = &pipeline->rast_info,
        .pMultisampleState = &pipeline->msaa_info,
        .pDepthStencilState = &pipeline->depth_info,
        .pColorBlendState = &color_info,
        .layout = pipeline->pipeline_layout,
        .renderPass = pipeline->fb ? pipeline->fb->pass : VK_NULL_HANDLE,
    };

    vk->result = vk->CreateGraphicsPipelines(vk->dev, VK_NULL_HANDLE, 1, &pipeline_info, NULL,
                                             &pipeline->pipeline);
    vk_check(vk, "failed to create graphics pipeline");
}

static inline void
vk_destroy_pipeline(struct vk *vk, struct vk_pipeline *pipeline)
{
    for (uint32_t i = 0; i < pipeline->stage_count; i++)
        vk->DestroyShaderModule(vk->dev, pipeline->stages[i].module, NULL);

    for (uint32_t i = 0; i < pipeline->set_layout_count; i++)
        vk->DestroyDescriptorSetLayout(vk->dev, pipeline->set_layouts[i], NULL);

    vk->DestroyPipelineLayout(vk->dev, pipeline->pipeline_layout, NULL);

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
vk_create_semaphore(struct vk *vk, VkSemaphoreType type)
{
    struct vk_semaphore *sem = (struct vk_semaphore *)calloc(1, sizeof(*sem));
    if (!sem)
        vk_die("failed to alloc semaphore");

    if (type == VK_SEMAPHORE_TYPE_TIMELINE &&
        !(vk->vulkan_12_features.timelineSemaphore && vk->params.enable_all_features))
        vk_die("no support for timeline semaphore");

    const VkSemaphoreTypeCreateInfo type_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
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
    struct vk_stopwatch *stopwatch = calloc(1, sizeof(*stopwatch));
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

    vk->CmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, stopwatch->query->pool,
                          stopwatch->query_count++);
}

static inline uint64_t
vk_read_stopwatch(struct vk *vk, struct vk_stopwatch *stopwatch, uint32_t idx)
{
    if (!stopwatch->ts) {
        stopwatch->ts = malloc(sizeof(*stopwatch->ts) * stopwatch->query_count);
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
    VkFence *fence = &vk->submit.fences[vk->submit.next];
    bool *protected_submit = &vk->submit.protected_submits[vk->submit.next];

    /* reuse or allocate */
    if (*cmd && *protected_submit == prot) {
        vk->result = vk->WaitForFences(vk->dev, 1, fence, true, UINT64_MAX);
        vk_check(vk, "failed to wait fence");

        vk->result = vk->ResetCommandBuffer(*cmd, 0);
        vk_check(vk, "failed to reset command buffer");

        vk->result = vk->ResetFences(vk->dev, 1, fence);
        vk_check(vk, "failed to reset fence");
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

        const VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        };
        vk->result = vk->CreateFence(vk->dev, &fence_info, NULL, fence);
        vk_check(vk, "failed to create fence");

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
    VkFence fence = vk->submit.fences[vk->submit.next];
    bool protected_submit = vk->submit.protected_submits[vk->submit.next];

    /* increment */
    vk->submit.next = (vk->submit.next + 1) % vk->submit.count;

    vk->result = vk->EndCommandBuffer(cmd);
    vk_check(vk, "failed to end command buffer");

    const VkProtectedSubmitInfo protected_info = {
        .sType = VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO,
        .protectedSubmit = protected_submit,
    };
    const VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = &protected_info,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };
    vk->result = vk->QueueSubmit(vk->queue, 1, &submit_info, fence);
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

        VkFormatProperties2 fmt_props = {
            .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
        };
        vk->GetPhysicalDeviceFormatProperties2(vk->physical_dev, img->info.format, &fmt_props);
        img->features = img->info.tiling == VK_IMAGE_TILING_OPTIMAL
                            ? fmt_props.formatProperties.optimalTilingFeatures
                            : fmt_props.formatProperties.linearTilingFeatures;
        vk_validate_image(vk, img);

        img->img = swapchain->img_handles[i];
    }
}

static inline struct vk_swapchain *
vk_create_swapchain(struct vk *vk,
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
