/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

/* This test lists all formats supported by the device.  */

#include "vkutil.h"

struct formats_test_format {
    VkFormat format;
    const char *name;
};

struct formats_test_name {
    uint64_t bits;
    const char *name;
};

static const struct formats_test_format formats_test_formats[] = {
#define FMT(fmt) { VK_FORMAT_##fmt, "VK_FORMAT_" #fmt },
#include "vkutil_formats.inc"
};

static const VkExternalMemoryHandleTypeFlagBits formats_test_handles[] = {
    0,
#ifdef __ANDROID__
    VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
#else
    VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
#endif
};

static const VkImageType formats_test_types[] = {
    VK_IMAGE_TYPE_1D,
    VK_IMAGE_TYPE_2D,
    VK_IMAGE_TYPE_3D,
};

static const VkImageTiling formats_test_tilings[] = {
    VK_IMAGE_TILING_LINEAR,
    VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
};

static const VkImageUsageFlagBits formats_test_usages[] = {
    VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    VK_IMAGE_USAGE_SAMPLED_BIT,
    VK_IMAGE_USAGE_STORAGE_BIT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
    VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
    VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
    VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT,
    VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR,
    VK_IMAGE_USAGE_VIDEO_DECODE_SRC_BIT_KHR,
    VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR,
#if 0
    VK_IMAGE_USAGE_VIDEO_ENCODE_DST_BIT_KHR,
    VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR,
    VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR,
    VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT,
#endif
};

static const struct formats_test_name formats_test_usage_names[] = {
    { VK_IMAGE_USAGE_TRANSFER_SRC_BIT, "xfers" },
    { VK_IMAGE_USAGE_TRANSFER_DST_BIT, "xferd" },
    { VK_IMAGE_USAGE_SAMPLED_BIT, "sampled" },
    { VK_IMAGE_USAGE_STORAGE_BIT, "storage" },
    { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, "color" },
    { VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, "depth" },
    { VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT, "transient" },
    { VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, "input" },
};

static const struct formats_test_name formats_test_feature_names[] = {
    { VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, "sampled" },
    { VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT, "storage" },
    { VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT, "atomic" },
    { VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT, "sampled" },
    { VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT, "storage" },
    { VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT, "atomic" },
    { VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT, "vertex" },
    { VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT, "color" },
    { VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT, "blend" },
    { VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, "depth" },
    { VK_FORMAT_FEATURE_BLIT_SRC_BIT, "blits" },
    { VK_FORMAT_FEATURE_BLIT_DST_BIT, "blitd" },
    { VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT, "filtering" },
    { VK_FORMAT_FEATURE_TRANSFER_SRC_BIT, "xfers" },
    { VK_FORMAT_FEATURE_TRANSFER_DST_BIT, "xferd" },
    { VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT, "midpoint" },
    { VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT, "chroma" },
    { VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT,
      "separate" },
    { VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT,
      "explicit" },
    { VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT,
      "forceable" },
    { VK_FORMAT_FEATURE_DISJOINT_BIT, "disjoint" },
    { VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT, "cosited" },
    { VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT, "minmax" },
};

static void
formats_get_str(
    uint64_t bits, const struct formats_test_name *names, uint32_t count, char *str, size_t size)
{
    int len = 0;
    for (uint32_t i = 0; i < count; i++) {
        const struct formats_test_name *name = &names[i];
        if (bits & name->bits) {
            len += snprintf(str + len, size - len, "%s|", name->name);
            bits &= ~name->bits;
        }
    }

    if (bits)
        snprintf(str + len, size - len, "0x%" PRIx64, bits);
    else if (len)
        str[len - 1] = '\0';
    else
        snprintf(str + len, size - len, "none");
}

static void
formats_get_usage_str(VkImageUsageFlags usage, char *str, size_t size)
{
    formats_get_str(usage, formats_test_usage_names, ARRAY_SIZE(formats_test_usage_names), str,
                    size);
}

static void
formats_get_feature_str(VkFormatFeatureFlags features, char *str, size_t size)
{
    formats_get_str(features, formats_test_feature_names, ARRAY_SIZE(formats_test_feature_names),
                    str, size);
}

static void
formats_get_tiling_str(VkImageTiling tiling, uint64_t drm_modifier, char *str, size_t size)
{
    if (tiling == VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT)
        snprintf(str, size, "modifier 0x%016" PRIx64, drm_modifier);
    else
        snprintf(str, size, "%s", tiling == VK_IMAGE_TILING_LINEAR ? "linear" : "optimal");
}

static const char *
formats_get_type_str(VkImageType type)
{
    switch (type) {
    case VK_IMAGE_TYPE_1D:
        return "1d";
    case VK_IMAGE_TYPE_2D:
        return "2d";
    case VK_IMAGE_TYPE_3D:
        return "3d";
    default:
        vk_die("bad image type");
    }
}

static void
formats_test_dump_image_format(struct vk *vk,
                               const struct formats_test_format *fmt,
                               VkExternalMemoryHandleTypeFlagBits handle,
                               VkImageType type,
                               VkImageTiling tiling,
                               uint64_t drm_modifier)
{
    const VkPhysicalDeviceImageDrmFormatModifierInfoEXT mod_info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
        .drmFormatModifier = drm_modifier,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkPhysicalDeviceExternalImageFormatInfo external_info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
        .pNext = tiling == VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT ? &mod_info : NULL,
        .handleType = handle,
    };
    VkPhysicalDeviceImageFormatInfo2 info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
        .pNext = &external_info,
        .format = fmt->format,
        .type = type,
        .tiling = tiling,
        .usage = 0, /* TBD */
        .flags = 0,
    };

    VkAndroidHardwareBufferUsageANDROID ahb_props = {
        .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID,
    };
    VkExternalImageFormatProperties external_props = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
        .pNext = &ahb_props,
    };
    VkSamplerYcbcrConversionImageFormatProperties ycbcr_props = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES,
        .pNext = &external_props,
    };
    VkImageFormatProperties2 props = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
        .pNext = &ycbcr_props,
    };

    VkImageUsageFlags usage = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(formats_test_usages); i++) {
        info.usage = formats_test_usages[i];
        const VkResult result =
            vk->GetPhysicalDeviceImageFormatProperties2(vk->physical_dev, &info, &props);
        if (result == VK_SUCCESS)
            usage |= info.usage;
    }
    if (!usage)
        return;

    const char *type_str = formats_get_type_str(type);
    char tiling_str[64];
    char usage_str[128];
    formats_get_tiling_str(tiling, drm_modifier, tiling_str, ARRAY_SIZE(tiling_str));
    formats_get_usage_str(usage, usage_str, ARRAY_SIZE(usage_str));

    if (handle)
        vk_log("  %s image, %s, external handle 0x%x", type_str, tiling_str, handle);
    else
        vk_log("  %s image, %s", type_str, tiling_str);

    vk_log("    usage: %s", usage_str);
    vk_log("    maxExtent: [%d, %d, %d]", props.imageFormatProperties.maxExtent.width,
           props.imageFormatProperties.maxExtent.height,
           props.imageFormatProperties.maxExtent.depth);
    vk_log("    maxMipLevels: %d", props.imageFormatProperties.maxMipLevels);
    vk_log("    maxArrayLayers: %d", props.imageFormatProperties.maxArrayLayers);
    vk_log("    sampleCounts: 0x%x", props.imageFormatProperties.sampleCounts);
    if (ycbcr_props.combinedImageSamplerDescriptorCount > 1)
        vk_log("    combinedImageSamplerDescriptorCount: %d",
               ycbcr_props.combinedImageSamplerDescriptorCount);

    if (handle) {
        const VkExternalMemoryProperties *mem_props = &external_props.externalMemoryProperties;
        vk_log("    externalMemoryFeatures: 0x%x", mem_props->externalMemoryFeatures);

        if (handle == VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID) {
            vk_log("    androidHardwareBufferUsage: 0x%" PRIx64,
                   ahb_props.androidHardwareBufferUsage);
        }
    }
}

static void
formats_test_dump_format(struct vk *vk, const struct formats_test_format *fmt)
{
    VkDrmFormatModifierPropertiesListEXT mod_props = {
        .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
    };
    VkFormatProperties2 props = {
        .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
        .pNext = &mod_props,
    };
    vk->GetPhysicalDeviceFormatProperties2(vk->physical_dev, fmt->format, &props);

    if (mod_props.drmFormatModifierCount) {
        mod_props.pDrmFormatModifierProperties = malloc(
            sizeof(*mod_props.pDrmFormatModifierProperties) * mod_props.drmFormatModifierCount);
        if (!mod_props.pDrmFormatModifierProperties)
            vk_die("failed to alloc VkDrmFormatModifierPropertiesEXT");
        vk->GetPhysicalDeviceFormatProperties2(vk->physical_dev, fmt->format, &props);
    }

    const bool can_buffer = props.formatProperties.bufferFeatures;
    const bool can_img = props.formatProperties.linearTilingFeatures ||
                         props.formatProperties.optimalTilingFeatures ||
                         mod_props.drmFormatModifierCount;

    if (!can_buffer && !can_img) {
        vk_log("%s is not supported", fmt->name);
        free(mod_props.pDrmFormatModifierProperties);
        return;
    }

    vk_log("%s", fmt->name);

    char features_str[128];
    if (can_buffer) {
        formats_get_feature_str(props.formatProperties.bufferFeatures, features_str,
                                ARRAY_SIZE(features_str));
        vk_log("  bufferFeatures: %s", features_str);
    }

    if (!can_img) {
        free(mod_props.pDrmFormatModifierProperties);
        return;
    }

    formats_get_feature_str(props.formatProperties.linearTilingFeatures, features_str,
                            ARRAY_SIZE(features_str));
    vk_log("  linearTilingFeatures: %s", features_str);

    formats_get_feature_str(props.formatProperties.optimalTilingFeatures, features_str,
                            ARRAY_SIZE(features_str));
    vk_log("  optimalTilingFeatures: %s", features_str);

    for (uint32_t i = 0; i < mod_props.drmFormatModifierCount; i++) {
        const VkDrmFormatModifierPropertiesEXT *p = &mod_props.pDrmFormatModifierProperties[i];
        char tiling_str[32];

        formats_get_tiling_str(VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT, p->drmFormatModifier,
                               tiling_str, ARRAY_SIZE(tiling_str));
        formats_get_feature_str(p->drmFormatModifierTilingFeatures, features_str,
                                ARRAY_SIZE(features_str));
        vk_log("  %s features: %s, plane count %d", tiling_str, features_str,
               p->drmFormatModifierPlaneCount);
    }

    for (uint32_t i = 0; i < ARRAY_SIZE(formats_test_handles); i++) {
        for (uint32_t j = 0; j < ARRAY_SIZE(formats_test_types); j++) {
            for (uint32_t k = 0; k < ARRAY_SIZE(formats_test_tilings); k++) {
                const VkExternalMemoryHandleTypeFlagBits handle = formats_test_handles[i];
                const VkImageType type = formats_test_types[j];
                const VkImageTiling tiling = formats_test_tilings[k];

                if (tiling == VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT) {
                    for (uint32_t i = 0; i < mod_props.drmFormatModifierCount; i++) {
                        const VkDrmFormatModifierPropertiesEXT *p =
                            &mod_props.pDrmFormatModifierProperties[i];
                        formats_test_dump_image_format(vk, fmt, handle, type, tiling,
                                                       p->drmFormatModifier);
                    }
                } else {
                    formats_test_dump_image_format(vk, fmt, handle, type, tiling,
                                                   DRM_FORMAT_MOD_INVALID);
                }
            }
        }
    }

    free(mod_props.pDrmFormatModifierProperties);
}

static void
formats_test_dump(struct vk *vk)
{
    for (uint32_t i = 0; i < ARRAY_SIZE(formats_test_formats); i++) {
        const struct formats_test_format *fmt = &formats_test_formats[i];
        formats_test_dump_format(vk, fmt);
    }
}

int
main(void)
{
    struct vk vk;
    vk_init(&vk, NULL);
    formats_test_dump(&vk);
    vk_cleanup(&vk);

    return 0;
}
