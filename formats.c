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

static const struct formats_test_format formats_test_formats[] = {
#define FMT(fmt)                                                                                 \
    {                                                                                            \
        fmt, #fmt                                                                                \
    }
    FMT(VK_FORMAT_UNDEFINED),
    FMT(VK_FORMAT_R4G4_UNORM_PACK8),
    FMT(VK_FORMAT_R4G4B4A4_UNORM_PACK16),
    FMT(VK_FORMAT_B4G4R4A4_UNORM_PACK16),
    FMT(VK_FORMAT_R5G6B5_UNORM_PACK16),
    FMT(VK_FORMAT_B5G6R5_UNORM_PACK16),
    FMT(VK_FORMAT_R5G5B5A1_UNORM_PACK16),
    FMT(VK_FORMAT_B5G5R5A1_UNORM_PACK16),
    FMT(VK_FORMAT_A1R5G5B5_UNORM_PACK16),
    FMT(VK_FORMAT_R8_UNORM),
    FMT(VK_FORMAT_R8_SNORM),
    FMT(VK_FORMAT_R8_USCALED),
    FMT(VK_FORMAT_R8_SSCALED),
    FMT(VK_FORMAT_R8_UINT),
    FMT(VK_FORMAT_R8_SINT),
    FMT(VK_FORMAT_R8_SRGB),
    FMT(VK_FORMAT_R8G8_UNORM),
    FMT(VK_FORMAT_R8G8_SNORM),
    FMT(VK_FORMAT_R8G8_USCALED),
    FMT(VK_FORMAT_R8G8_SSCALED),
    FMT(VK_FORMAT_R8G8_UINT),
    FMT(VK_FORMAT_R8G8_SINT),
    FMT(VK_FORMAT_R8G8_SRGB),
    FMT(VK_FORMAT_R8G8B8_UNORM),
    FMT(VK_FORMAT_R8G8B8_SNORM),
    FMT(VK_FORMAT_R8G8B8_USCALED),
    FMT(VK_FORMAT_R8G8B8_SSCALED),
    FMT(VK_FORMAT_R8G8B8_UINT),
    FMT(VK_FORMAT_R8G8B8_SINT),
    FMT(VK_FORMAT_R8G8B8_SRGB),
    FMT(VK_FORMAT_B8G8R8_UNORM),
    FMT(VK_FORMAT_B8G8R8_SNORM),
    FMT(VK_FORMAT_B8G8R8_USCALED),
    FMT(VK_FORMAT_B8G8R8_SSCALED),
    FMT(VK_FORMAT_B8G8R8_UINT),
    FMT(VK_FORMAT_B8G8R8_SINT),
    FMT(VK_FORMAT_B8G8R8_SRGB),
    FMT(VK_FORMAT_R8G8B8A8_UNORM),
    FMT(VK_FORMAT_R8G8B8A8_SNORM),
    FMT(VK_FORMAT_R8G8B8A8_USCALED),
    FMT(VK_FORMAT_R8G8B8A8_SSCALED),
    FMT(VK_FORMAT_R8G8B8A8_UINT),
    FMT(VK_FORMAT_R8G8B8A8_SINT),
    FMT(VK_FORMAT_R8G8B8A8_SRGB),
    FMT(VK_FORMAT_B8G8R8A8_UNORM),
    FMT(VK_FORMAT_B8G8R8A8_SNORM),
    FMT(VK_FORMAT_B8G8R8A8_USCALED),
    FMT(VK_FORMAT_B8G8R8A8_SSCALED),
    FMT(VK_FORMAT_B8G8R8A8_UINT),
    FMT(VK_FORMAT_B8G8R8A8_SINT),
    FMT(VK_FORMAT_B8G8R8A8_SRGB),
    FMT(VK_FORMAT_A8B8G8R8_UNORM_PACK32),
    FMT(VK_FORMAT_A8B8G8R8_SNORM_PACK32),
    FMT(VK_FORMAT_A8B8G8R8_USCALED_PACK32),
    FMT(VK_FORMAT_A8B8G8R8_SSCALED_PACK32),
    FMT(VK_FORMAT_A8B8G8R8_UINT_PACK32),
    FMT(VK_FORMAT_A8B8G8R8_SINT_PACK32),
    FMT(VK_FORMAT_A8B8G8R8_SRGB_PACK32),
    FMT(VK_FORMAT_A2R10G10B10_UNORM_PACK32),
    FMT(VK_FORMAT_A2R10G10B10_SNORM_PACK32),
    FMT(VK_FORMAT_A2R10G10B10_USCALED_PACK32),
    FMT(VK_FORMAT_A2R10G10B10_SSCALED_PACK32),
    FMT(VK_FORMAT_A2R10G10B10_UINT_PACK32),
    FMT(VK_FORMAT_A2R10G10B10_SINT_PACK32),
    FMT(VK_FORMAT_A2B10G10R10_UNORM_PACK32),
    FMT(VK_FORMAT_A2B10G10R10_SNORM_PACK32),
    FMT(VK_FORMAT_A2B10G10R10_USCALED_PACK32),
    FMT(VK_FORMAT_A2B10G10R10_SSCALED_PACK32),
    FMT(VK_FORMAT_A2B10G10R10_UINT_PACK32),
    FMT(VK_FORMAT_A2B10G10R10_SINT_PACK32),
    FMT(VK_FORMAT_R16_UNORM),
    FMT(VK_FORMAT_R16_SNORM),
    FMT(VK_FORMAT_R16_USCALED),
    FMT(VK_FORMAT_R16_SSCALED),
    FMT(VK_FORMAT_R16_UINT),
    FMT(VK_FORMAT_R16_SINT),
    FMT(VK_FORMAT_R16_SFLOAT),
    FMT(VK_FORMAT_R16G16_UNORM),
    FMT(VK_FORMAT_R16G16_SNORM),
    FMT(VK_FORMAT_R16G16_USCALED),
    FMT(VK_FORMAT_R16G16_SSCALED),
    FMT(VK_FORMAT_R16G16_UINT),
    FMT(VK_FORMAT_R16G16_SINT),
    FMT(VK_FORMAT_R16G16_SFLOAT),
    FMT(VK_FORMAT_R16G16B16_UNORM),
    FMT(VK_FORMAT_R16G16B16_SNORM),
    FMT(VK_FORMAT_R16G16B16_USCALED),
    FMT(VK_FORMAT_R16G16B16_SSCALED),
    FMT(VK_FORMAT_R16G16B16_UINT),
    FMT(VK_FORMAT_R16G16B16_SINT),
    FMT(VK_FORMAT_R16G16B16_SFLOAT),
    FMT(VK_FORMAT_R16G16B16A16_UNORM),
    FMT(VK_FORMAT_R16G16B16A16_SNORM),
    FMT(VK_FORMAT_R16G16B16A16_USCALED),
    FMT(VK_FORMAT_R16G16B16A16_SSCALED),
    FMT(VK_FORMAT_R16G16B16A16_UINT),
    FMT(VK_FORMAT_R16G16B16A16_SINT),
    FMT(VK_FORMAT_R16G16B16A16_SFLOAT),
    FMT(VK_FORMAT_R32_UINT),
    FMT(VK_FORMAT_R32_SINT),
    FMT(VK_FORMAT_R32_SFLOAT),
    FMT(VK_FORMAT_R32G32_UINT),
    FMT(VK_FORMAT_R32G32_SINT),
    FMT(VK_FORMAT_R32G32_SFLOAT),
    FMT(VK_FORMAT_R32G32B32_UINT),
    FMT(VK_FORMAT_R32G32B32_SINT),
    FMT(VK_FORMAT_R32G32B32_SFLOAT),
    FMT(VK_FORMAT_R32G32B32A32_UINT),
    FMT(VK_FORMAT_R32G32B32A32_SINT),
    FMT(VK_FORMAT_R32G32B32A32_SFLOAT),
    FMT(VK_FORMAT_R64_UINT),
    FMT(VK_FORMAT_R64_SINT),
    FMT(VK_FORMAT_R64_SFLOAT),
    FMT(VK_FORMAT_R64G64_UINT),
    FMT(VK_FORMAT_R64G64_SINT),
    FMT(VK_FORMAT_R64G64_SFLOAT),
    FMT(VK_FORMAT_R64G64B64_UINT),
    FMT(VK_FORMAT_R64G64B64_SINT),
    FMT(VK_FORMAT_R64G64B64_SFLOAT),
    FMT(VK_FORMAT_R64G64B64A64_UINT),
    FMT(VK_FORMAT_R64G64B64A64_SINT),
    FMT(VK_FORMAT_R64G64B64A64_SFLOAT),
    FMT(VK_FORMAT_B10G11R11_UFLOAT_PACK32),
    FMT(VK_FORMAT_E5B9G9R9_UFLOAT_PACK32),
    FMT(VK_FORMAT_D16_UNORM),
    FMT(VK_FORMAT_X8_D24_UNORM_PACK32),
    FMT(VK_FORMAT_D32_SFLOAT),
    FMT(VK_FORMAT_S8_UINT),
    FMT(VK_FORMAT_D16_UNORM_S8_UINT),
    FMT(VK_FORMAT_D24_UNORM_S8_UINT),
    FMT(VK_FORMAT_D32_SFLOAT_S8_UINT),
    FMT(VK_FORMAT_BC1_RGB_UNORM_BLOCK),
    FMT(VK_FORMAT_BC1_RGB_SRGB_BLOCK),
    FMT(VK_FORMAT_BC1_RGBA_UNORM_BLOCK),
    FMT(VK_FORMAT_BC1_RGBA_SRGB_BLOCK),
    FMT(VK_FORMAT_BC2_UNORM_BLOCK),
    FMT(VK_FORMAT_BC2_SRGB_BLOCK),
    FMT(VK_FORMAT_BC3_UNORM_BLOCK),
    FMT(VK_FORMAT_BC3_SRGB_BLOCK),
    FMT(VK_FORMAT_BC4_UNORM_BLOCK),
    FMT(VK_FORMAT_BC4_SNORM_BLOCK),
    FMT(VK_FORMAT_BC5_UNORM_BLOCK),
    FMT(VK_FORMAT_BC5_SNORM_BLOCK),
    FMT(VK_FORMAT_BC6H_UFLOAT_BLOCK),
    FMT(VK_FORMAT_BC6H_SFLOAT_BLOCK),
    FMT(VK_FORMAT_BC7_UNORM_BLOCK),
    FMT(VK_FORMAT_BC7_SRGB_BLOCK),
    FMT(VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK),
    FMT(VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK),
    FMT(VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK),
    FMT(VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK),
    FMT(VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK),
    FMT(VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK),
    FMT(VK_FORMAT_EAC_R11_UNORM_BLOCK),
    FMT(VK_FORMAT_EAC_R11_SNORM_BLOCK),
    FMT(VK_FORMAT_EAC_R11G11_UNORM_BLOCK),
    FMT(VK_FORMAT_EAC_R11G11_SNORM_BLOCK),
    FMT(VK_FORMAT_ASTC_4x4_UNORM_BLOCK),
    FMT(VK_FORMAT_ASTC_4x4_SRGB_BLOCK),
    FMT(VK_FORMAT_ASTC_5x4_UNORM_BLOCK),
    FMT(VK_FORMAT_ASTC_5x4_SRGB_BLOCK),
    FMT(VK_FORMAT_ASTC_5x5_UNORM_BLOCK),
    FMT(VK_FORMAT_ASTC_5x5_SRGB_BLOCK),
    FMT(VK_FORMAT_ASTC_6x5_UNORM_BLOCK),
    FMT(VK_FORMAT_ASTC_6x5_SRGB_BLOCK),
    FMT(VK_FORMAT_ASTC_6x6_UNORM_BLOCK),
    FMT(VK_FORMAT_ASTC_6x6_SRGB_BLOCK),
    FMT(VK_FORMAT_ASTC_8x5_UNORM_BLOCK),
    FMT(VK_FORMAT_ASTC_8x5_SRGB_BLOCK),
    FMT(VK_FORMAT_ASTC_8x6_UNORM_BLOCK),
    FMT(VK_FORMAT_ASTC_8x6_SRGB_BLOCK),
    FMT(VK_FORMAT_ASTC_8x8_UNORM_BLOCK),
    FMT(VK_FORMAT_ASTC_8x8_SRGB_BLOCK),
    FMT(VK_FORMAT_ASTC_10x5_UNORM_BLOCK),
    FMT(VK_FORMAT_ASTC_10x5_SRGB_BLOCK),
    FMT(VK_FORMAT_ASTC_10x6_UNORM_BLOCK),
    FMT(VK_FORMAT_ASTC_10x6_SRGB_BLOCK),
    FMT(VK_FORMAT_ASTC_10x8_UNORM_BLOCK),
    FMT(VK_FORMAT_ASTC_10x8_SRGB_BLOCK),
    FMT(VK_FORMAT_ASTC_10x10_UNORM_BLOCK),
    FMT(VK_FORMAT_ASTC_10x10_SRGB_BLOCK),
    FMT(VK_FORMAT_ASTC_12x10_UNORM_BLOCK),
    FMT(VK_FORMAT_ASTC_12x10_SRGB_BLOCK),
    FMT(VK_FORMAT_ASTC_12x12_UNORM_BLOCK),
    FMT(VK_FORMAT_ASTC_12x12_SRGB_BLOCK),
    FMT(VK_FORMAT_G8B8G8R8_422_UNORM),
    FMT(VK_FORMAT_B8G8R8G8_422_UNORM),
    FMT(VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM),
    FMT(VK_FORMAT_G8_B8R8_2PLANE_420_UNORM),
    FMT(VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM),
    FMT(VK_FORMAT_G8_B8R8_2PLANE_422_UNORM),
    FMT(VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM),
    FMT(VK_FORMAT_R10X6_UNORM_PACK16),
    FMT(VK_FORMAT_R10X6G10X6_UNORM_2PACK16),
    FMT(VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16),
    FMT(VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16),
    FMT(VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16),
    FMT(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16),
    FMT(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16),
    FMT(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16),
    FMT(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16),
    FMT(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16),
    FMT(VK_FORMAT_R12X4_UNORM_PACK16),
    FMT(VK_FORMAT_R12X4G12X4_UNORM_2PACK16),
    FMT(VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16),
    FMT(VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16),
    FMT(VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16),
    FMT(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16),
    FMT(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16),
    FMT(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16),
    FMT(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16),
    FMT(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16),
    FMT(VK_FORMAT_G16B16G16R16_422_UNORM),
    FMT(VK_FORMAT_B16G16R16G16_422_UNORM),
    FMT(VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM),
    FMT(VK_FORMAT_G16_B16R16_2PLANE_420_UNORM),
    FMT(VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM),
    FMT(VK_FORMAT_G16_B16R16_2PLANE_422_UNORM),
    FMT(VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM),
    FMT(VK_FORMAT_G8_B8R8_2PLANE_444_UNORM),
    FMT(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16),
    FMT(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16),
    FMT(VK_FORMAT_G16_B16R16_2PLANE_444_UNORM),
    FMT(VK_FORMAT_A4R4G4B4_UNORM_PACK16),
    FMT(VK_FORMAT_A4B4G4R4_UNORM_PACK16),
    FMT(VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK),
    FMT(VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK),
    FMT(VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK),
    FMT(VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK),
    FMT(VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK),
    FMT(VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK),
    FMT(VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK),
    FMT(VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK),
    FMT(VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK),
    FMT(VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK),
    FMT(VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK),
    FMT(VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK),
    FMT(VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK),
    FMT(VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK),
#undef FMT
};

static const VkExternalMemoryHandleTypeFlagBits formats_test_handles[] = {
    0,
#ifdef __ANDROID__
    VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
#else
    VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
#endif
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

static void
formats_test_dump_image_format(struct vk *vk,
                               VkFormat format,
                               VkExternalMemoryHandleTypeFlagBits handle,
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
        .type = VK_IMAGE_TYPE_2D,
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
    VkImageFormatProperties2 props = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
        .pNext = &external_props,
    };

    const VkResult result =
        vk->GetPhysicalDeviceImageFormatProperties2(vk->physical_dev, &info, &props);
    if (result == VK_SUCCESS) {
        vk_log("    supported: true");

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

    if (can_buffer)
        vk_log("  bufferFeatures 0x%08x", props.formatProperties.bufferFeatures);

    if (!can_img) {
        if (!can_buffer)
            free(mod_props.pDrmFormatModifierProperties);
        return;
    }

    vk_log("  linearTilingFeatures 0x%08x", props.formatProperties.linearTilingFeatures);
    vk_log("  optimalTilingFeatures 0x%08x", props.formatProperties.optimalTilingFeatures);

    for (uint32_t i = 0; i < mod_props.drmFormatModifierCount; i++) {
        const VkDrmFormatModifierPropertiesEXT *p = &mod_props.pDrmFormatModifierProperties[i];
        vk_log("  modifier 0x%016" PRIx64 ": 0x%08x plane count %d", p->drmFormatModifier,
               p->drmFormatModifierTilingFeatures, p->drmFormatModifierPlaneCount);
    }

    for (uint32_t h = 0; h < ARRAY_SIZE(formats_test_handles); h++) {
        for (uint32_t t = 0; t < ARRAY_SIZE(formats_test_tilings); t++) {
            for (uint32_t u = 0; u < ARRAY_SIZE(formats_test_usages); u++) {
                const VkExternalMemoryHandleTypeFlagBits handle = formats_test_handles[h];
                const VkImageTiling tiling = formats_test_tilings[t];
                const VkImageUsageFlags usage = formats_test_usages[u];

                if (tiling == VK_IMAGE_TILING_OPTIMAL || tiling == VK_IMAGE_TILING_LINEAR) {
                    vk_log("  external handle 0x%x, image tiling %s, usage 0x%08x", handle,
                           tiling == VK_IMAGE_TILING_OPTIMAL ? "optimal" : "linear", usage);
                    formats_test_dump_image_format(vk, format, handle, tiling,
                                                   DRM_FORMAT_MOD_INVALID, usage);
                } else {
                    for (uint32_t i = 0; i < mod_props.drmFormatModifierCount; i++) {
                        const VkDrmFormatModifierPropertiesEXT *p =
                            &mod_props.pDrmFormatModifierProperties[i];
                        vk_log("  external handle 0x%x, image modifier 0x%016" PRIx64
                               ", usage 0x%08x",
                               handle, p->drmFormatModifier, usage);
                        formats_test_dump_image_format(vk, format, handle,
                                                       VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
                                                       p->drmFormatModifier, usage);
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
    vk_init(&vk);
    formats_test_dump(&vk);
    vk_cleanup(&vk);

    return 0;
}
