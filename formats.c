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
    VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_TILING_LINEAR,
    VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
};

static const VkImageUsageFlags formats_test_usages[] = {
    VK_IMAGE_USAGE_SAMPLED_BIT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
};

static const struct formats_test_name formats_test_usage_names[] = {
    { VK_IMAGE_USAGE_SAMPLED_BIT, "sampled" },
    { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, "color" },
    { VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, "depth" },
};

static const struct formats_test_name formats_test_feature_names[] = {
    { VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, "sampled" },
    { VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT, "color" },
    { VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, "depth" },
    { VK_FORMAT_FEATURE_TRANSFER_SRC_BIT, "xfers" },
    { VK_FORMAT_FEATURE_TRANSFER_DST_BIT, "xferd" },
    { VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT, "midpoint" },
    { VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT, "linear" },
    { VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT,
      "separate" },
    { VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT,
      "explicit" },
    { VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT,
      "forceable" },
    { VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT, "cosited" },
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
formats_test_dump_image_format(struct vk *vk,
                               VkFormat format,
                               VkExternalMemoryHandleTypeFlagBits handle,
                               VkImageType type,
                               VkImageTiling tiling,
                               uint64_t drm_modifier,
                               VkImageUsageFlags usage)
{
    const VkPhysicalDeviceImageDrmFormatModifierInfoEXT mod_info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
        .drmFormatModifier = drm_modifier,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    const VkPhysicalDeviceExternalImageFormatInfo external_info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
        .pNext = tiling == VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT ? &mod_info : NULL,
        .handleType = handle,
    };
    const VkPhysicalDeviceImageFormatInfo2 info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
        .pNext = &external_info,
        .format = format,
        .type = type,
        .tiling = tiling,
        .usage = usage,
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

    const VkResult result =
        vk->GetPhysicalDeviceImageFormatProperties2(vk->physical_dev, &info, &props);
    if (result == VK_SUCCESS) {
        vk_log("    supported: true (desc count %d)",
               ycbcr_props.combinedImageSamplerDescriptorCount);

        if (handle) {
            const VkExternalMemoryProperties *mem_props =
                &external_props.externalMemoryProperties;
            vk_log("    externalMemoryFeatures: 0x%x", mem_props->externalMemoryFeatures);
        }

        if (handle == VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID) {
            vk_log("    androidHardwareBufferUsage: 0x%" PRIx64,
                   ahb_props.androidHardwareBufferUsage);
        }
    } else {
        vk_log("    supported: false");
    }
}

static void
formats_test_dump_format(struct vk *vk, VkFormat format)
{
    VkDrmFormatModifierPropertiesListEXT mod_props = {
        .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
    };
    VkFormatProperties2 props = {
        .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
        .pNext = &mod_props,
    };
    vk->GetPhysicalDeviceFormatProperties2(vk->physical_dev, format, &props);

    if (mod_props.drmFormatModifierCount) {
        mod_props.pDrmFormatModifierProperties = malloc(
            sizeof(*mod_props.pDrmFormatModifierProperties) * mod_props.drmFormatModifierCount);
        if (!mod_props.pDrmFormatModifierProperties)
            vk_die("failed to alloc VkDrmFormatModifierPropertiesEXT");
        vk->GetPhysicalDeviceFormatProperties2(vk->physical_dev, format, &props);
    }

    const bool can_buffer = props.formatProperties.bufferFeatures;
    const bool can_img = props.formatProperties.linearTilingFeatures ||
                         props.formatProperties.optimalTilingFeatures ||
                         mod_props.drmFormatModifierCount;
    vk_log("  supported: %s", can_buffer || can_img ? "true" : "false");

    char features_str[128];
    if (can_buffer) {
        formats_get_feature_str(props.formatProperties.bufferFeatures, features_str,
                                ARRAY_SIZE(features_str));
        vk_log("  bufferFeatures: %s", features_str);
    }

    if (!can_img) {
        if (!can_buffer)
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
        formats_get_feature_str(p->drmFormatModifierTilingFeatures, features_str,
                                ARRAY_SIZE(features_str));
        vk_log("  modifier 0x%016" PRIx64 ": %s plane count %d", p->drmFormatModifier,
               features_str, p->drmFormatModifierPlaneCount);
    }

    for (uint32_t h = 0; h < ARRAY_SIZE(formats_test_handles); h++) {
        for (uint32_t u = 0; u < ARRAY_SIZE(formats_test_usages); u++) {
            for (uint32_t t = 0; t < ARRAY_SIZE(formats_test_tilings); t++) {
                for (uint32_t ty = 0; ty < ARRAY_SIZE(formats_test_types); ty++) {
                    const VkExternalMemoryHandleTypeFlagBits handle = formats_test_handles[h];
                    const VkImageUsageFlags usage = formats_test_usages[u];
                    const VkImageTiling tiling = formats_test_tilings[t];
                    const VkImageType type = formats_test_types[ty];

                    char usage_str[128];
                    formats_get_usage_str(usage, usage_str, ARRAY_SIZE(usage_str));

                    const char *type_str;
                    switch (type) {
                    case VK_IMAGE_TYPE_1D:
                        type_str = "1d";
                        break;
                    case VK_IMAGE_TYPE_2D:
                        type_str = "2d";
                        break;
                    case VK_IMAGE_TYPE_3D:
                        type_str = "3d";
                        break;
                    default:
                        type_str = "xd";
                        break;
                    }

                    if (tiling == VK_IMAGE_TILING_OPTIMAL || tiling == VK_IMAGE_TILING_LINEAR) {
                        vk_log("  external handle 0x%x, usage %s, %s %s image", handle, usage_str,
                               tiling == VK_IMAGE_TILING_OPTIMAL ? "optimal" : "linear",
                               type_str);
                        formats_test_dump_image_format(vk, format, handle, type, tiling,
                                                       DRM_FORMAT_MOD_INVALID, usage);
                    } else {
                        for (uint32_t i = 0; i < mod_props.drmFormatModifierCount; i++) {
                            const VkDrmFormatModifierPropertiesEXT *p =
                                &mod_props.pDrmFormatModifierProperties[i];
                            vk_log("  external handle 0x%x, usage %s, modifier 0x%016" PRIx64
                                   " %s image",
                                   handle, usage_str, p->drmFormatModifier, type_str);
                            formats_test_dump_image_format(
                                vk, format, handle, type, VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
                                p->drmFormatModifier, usage);
                        }
                    }
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

        vk_log("%s", fmt->name);
        formats_test_dump_format(vk, fmt->format);
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
