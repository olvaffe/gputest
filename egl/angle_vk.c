/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "eglutil.h"
#include "vkutil.h"

#ifndef EGL_ANGLE_device_vulkan
#define EGL_VULKAN_VERSION_ANGLE 0x34A8
#define EGL_VULKAN_INSTANCE_ANGLE 0x34A9
#define EGL_VULKAN_INSTANCE_EXTENSIONS_ANGLE 0x34AA
#define EGL_VULKAN_PHYSICAL_DEVICE_ANGLE 0x34AB
#define EGL_VULKAN_DEVICE_ANGLE 0x34AC
#define EGL_VULKAN_DEVICE_EXTENSIONS_ANGLE 0x34AD
#define EGL_VULKAN_FEATURES_ANGLE 0x34AE
#define EGL_VULKAN_QUEUE_ANGLE 0x34AF
#define EGL_VULKAN_QUEUE_FAMILIY_INDEX_ANGLE 0x34D0
#define EGL_VULKAN_GET_INSTANCE_PROC_ADDR 0x34D1
#endif

struct angle_test {
    struct egl egl;

    uint32_t version;
    VkInstance instance;
    const char *const *instance_exts;
    VkPhysicalDevice physical_dev;
    VkDevice dev;
    const char *const *dev_exts;
    const VkPhysicalDeviceFeatures2 *features;
    VkQueue queue;
    uint32_t queue_family_index;
    PFN_vkGetInstanceProcAddr GetInstanceProcAddr;

    PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
};

static void
angle_test_init_vk(struct angle_test *test)
{
    struct egl *egl = &test->egl;
    EGLAttrib val;

    if (!strstr(egl->dev_exts, "EGL_ANGLE_device_vulkan"))
        egl_die("no EGL_ANGLE_device_vulkan");

    if (!egl->QueryDeviceAttribEXT(egl->dev, EGL_VULKAN_VERSION_ANGLE, &val))
        egl_die("failed to query EGL_VULKAN_VERSION_ANGLE");
    test->version = val;

    if (!egl->QueryDeviceAttribEXT(egl->dev, EGL_VULKAN_INSTANCE_ANGLE, &val))
        egl_die("failed to query EGL_VULKAN_INSTANCE_ANGLE");
    test->instance = (VkInstance)val;

    if (!egl->QueryDeviceAttribEXT(egl->dev, EGL_VULKAN_INSTANCE_EXTENSIONS_ANGLE, &val))
        egl_die("failed to query EGL_VULKAN_INSTANCE_EXTENSIONS_ANGLE");
    test->instance_exts = (const char *const *)val;

    if (!egl->QueryDeviceAttribEXT(egl->dev, EGL_VULKAN_PHYSICAL_DEVICE_ANGLE, &val))
        egl_die("failed to query EGL_VULKAN_PHYSICAL_DEVICE_ANGLE");
    test->physical_dev = (VkPhysicalDevice)val;

    if (!egl->QueryDeviceAttribEXT(egl->dev, EGL_VULKAN_DEVICE_ANGLE, &val))
        egl_die("failed to query EGL_VULKAN_DEVICE_ANGLE");
    test->dev = (VkDevice)val;

    if (!egl->QueryDeviceAttribEXT(egl->dev, EGL_VULKAN_DEVICE_EXTENSIONS_ANGLE, &val))
        egl_die("failed to query EGL_VULKAN_DEVICE_EXTENSIONS_ANGLE");
    test->dev_exts = (const char *const *)val;

    if (!egl->QueryDeviceAttribEXT(egl->dev, EGL_VULKAN_FEATURES_ANGLE, &val))
        egl_die("failed to query EGL_VULKAN_FEATURES_ANGLE");
    test->features = (const VkPhysicalDeviceFeatures2 *)val;

    if (!egl->QueryDeviceAttribEXT(egl->dev, EGL_VULKAN_QUEUE_ANGLE, &val))
        egl_die("failed to query EGL_VULKAN_QUEUE_ANGLE");
    test->queue = (VkQueue)val;

    if (!egl->QueryDeviceAttribEXT(egl->dev, EGL_VULKAN_QUEUE_FAMILIY_INDEX_ANGLE, &val))
        egl_die("failed to query EGL_VULKAN_QUEUE_FAMILIY_INDEX_ANGLE");
    test->queue_family_index = (uint32_t)val;

    if (!egl->QueryDeviceAttribEXT(egl->dev, EGL_VULKAN_GET_INSTANCE_PROC_ADDR, &val))
        egl_die("failed to query EGL_VULKAN_GET_INSTANCE_PROC_ADDR");
    test->GetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)val;

    test->GetDeviceProcAddr =
        (PFN_vkGetDeviceProcAddr)test->GetInstanceProcAddr(test->instance, "vkGetDeviceProcAddr");
    test->GetDeviceProcAddr =
        (PFN_vkGetDeviceProcAddr)test->GetDeviceProcAddr(test->dev, "vkGetDeviceProcAddr");
}

static void
angle_test_init(struct angle_test *test)
{
    struct egl *egl = &test->egl;

    const struct egl_init_params params = {
#ifdef __ANDROID__
        .libegl_name = "libEGL_angle.so",
#else
        .libegl_name = LIBEGL_NAME,
#endif
    };
    egl_init(egl, &params);

    angle_test_init_vk(test);

    egl_check(egl, "init");
}

static void
angle_test_cleanup(struct angle_test *test)
{
    struct egl *egl = &test->egl;

    egl_check(egl, "cleanup");

    egl_cleanup(egl);
}

static void
angle_test_dump_features(struct angle_test *test)
{
    egl_log("selected features:");

    for (const VkBaseInStructure *pnext = (const VkBaseInStructure *)test->features; pnext; pnext = pnext->pNext) {
        switch (pnext->sType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT: {
            const VkPhysicalDeviceTransformFeedbackFeaturesEXT *feats = (const void *)pnext;
            egl_log("  transformFeedback: %d", feats->transformFeedback);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES: {
            const VkPhysicalDeviceDynamicRenderingFeatures *feats = (const void *)pnext;
            egl_log("  dynamicRendering: %d", feats->dynamicRendering);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2: {
            const VkPhysicalDeviceFeatures2 *feats = (const void *)pnext;
            egl_log("  geometryShader: %d", feats->features.geometryShader);
            egl_log("  tessellationShader: %d", feats->features.tessellationShader);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES: {
            const VkPhysicalDeviceProtectedMemoryFeatures *feats = (const void *)pnext;
            egl_log("  protectedMemory: %d", feats->protectedMemory);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_FEATURES_EXT: {
            const VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT *feats = (const void *)pnext;
            egl_log("  advancedBlendCoherentOperations: %d",
                    feats->advancedBlendCoherentOperations);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES: {
            const VkPhysicalDeviceSamplerYcbcrConversionFeatures *feats = (const void *)pnext;
            egl_log("  samplerYcbcrConversion: %d",
                    feats->samplerYcbcrConversion);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES: {
            const VkPhysicalDeviceTimelineSemaphoreFeatures *feats = (const void *)pnext;
            egl_log("  timelineSemaphore: %d", feats->timelineSemaphore);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES: {
            const VkPhysicalDeviceDynamicRenderingLocalReadFeatures *feats = (const void *)pnext;
            egl_log("  dynamicRenderingLocalRead: %d", feats->dynamicRenderingLocalRead);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES: {
            const VkPhysicalDeviceHostQueryResetFeatures *feats = (const void *)pnext;
            egl_log("  hostQueryReset: %d", feats->hostQueryReset);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT: {
            const VkPhysicalDeviceExtendedDynamicStateFeaturesEXT *feats = (const void *)pnext;
            egl_log("  extendedDynamicState: %d", feats->extendedDynamicState);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES: {
            const VkPhysicalDeviceHostImageCopyFeatures *feats = (const void *)pnext;
            egl_log("  hostImageCopy: %d", feats->hostImageCopy);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES: {
            const VkPhysicalDeviceSynchronization2Features *feats = (const void *)pnext;
            egl_log("  synchronization2: %d", feats->synchronization2);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT: {
            const VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT *feats = (const void *)pnext;
            egl_log("  graphicsPipelineLibrary: %d", feats->graphicsPipelineLibrary);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT: {
            const VkPhysicalDeviceFaultFeaturesEXT *feats = (const void *)pnext;
            egl_log("  deviceFault: %d", feats->deviceFault);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_EXT: {
            const VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT *feats = (const void *)pnext;
            egl_log("  multisampledRenderToSingleSampled: %d",
                    feats->multisampledRenderToSingleSampled);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT: {
            const VkPhysicalDeviceExtendedDynamicState2FeaturesEXT *feats = (const void *)pnext;
            egl_log("  extendedDynamicState2: %d", feats->extendedDynamicState2);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVES_GENERATED_QUERY_FEATURES_EXT: {
            const VkPhysicalDevicePrimitivesGeneratedQueryFeaturesEXT *feats = (const void *)pnext;
            egl_log("  primitivesGeneratedQuery: %d", feats->primitivesGeneratedQuery);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES: {
            const VkPhysicalDeviceGlobalPriorityQueryFeatures *feats = (const void *)pnext;
            egl_log("  globalPriorityQuery: %d", feats->globalPriorityQuery);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_PROTECTED_ACCESS_FEATURES: {
            const VkPhysicalDevicePipelineProtectedAccessFeatures *feats = (const void *)pnext;
            egl_log("  pipelineProtectedAccess: %d", feats->pipelineProtectedAccess);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FORMAT_RESOLVE_FEATURES_ANDROID: {
            const VkPhysicalDeviceExternalFormatResolveFeaturesANDROID *feats = (const void *)pnext;
            egl_log("  externalFormatResolve: %d", feats->externalFormatResolve);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFIED_IMAGE_LAYOUTS_FEATURES_KHR: {
            const VkPhysicalDeviceUnifiedImageLayoutsFeaturesKHR *feats = (const void *)pnext;
            egl_log("  unifiedImageLayouts: %d", feats->unifiedImageLayouts);
            break;
        }
        default:
            egl_log("  sType: %d", pnext->sType);
            break;
        }
    }
}

static void
angle_test_dump(struct angle_test *test)
{
    egl_log("version: %d.%d.%d", VK_API_VERSION_MAJOR(test->version),
            VK_API_VERSION_MINOR(test->version), VK_API_VERSION_PATCH(test->version));

    egl_log("instance: %p", test->instance);
    egl_log("instance extensions:");
    for (const char *const *ext = test->instance_exts; *ext; ext++)
        egl_log("  %s", *ext);

    egl_log("device: %p", test->dev);
    egl_log("device extensions:");
    for (const char *const *ext = test->dev_exts; *ext; ext++)
        egl_log("  %s", *ext);

    angle_test_dump_features(test);

    egl_log("queue: %p", test->queue);
    egl_log("queue family index: %d", test->queue_family_index);

    egl_log("GetInstanceProcAddr: %p", test->GetInstanceProcAddr);
    egl_log("GetDeviceProcAddr: %p", test->GetDeviceProcAddr);
}

int
main(void)
{
    struct angle_test test;

    angle_test_init(&test);
    angle_test_dump(&test);
    angle_test_cleanup(&test);

    return 0;
}
