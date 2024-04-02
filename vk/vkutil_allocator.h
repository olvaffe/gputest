/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef VKUTIL_ALLOCATOR_H
#define VKUTIL_ALLOCATOR_H

#include "vkutil.h"

#include <unistd.h>

/* limited by VK_IMAGE_ASPECT_MEMORY_PLANE_x_BIT_EXT */
#define VK_ALLOCATOR_MEMORY_PLANE_MAX 4

struct vk_allocator {
    struct vk vk;

    VkExternalMemoryHandleTypeFlagBits handle_type;
};

struct vk_allocator_buffer_info {
    VkBufferCreateFlags flags;
    VkBufferUsageFlags usage;

    uint32_t mt_mask;
    bool mt_coherent;
};

struct vk_allocator_image_info {
    VkImageCreateFlags flags;
    VkFormat format;
    uint64_t modifier;
    uint32_t mem_plane_count;
    VkImageUsageFlags usage;
    VkImageCompressionFlagBitsEXT compression;

    uint32_t mt_mask;
    bool mt_coherent;
};

struct vk_allocator_bo {
    bool is_img;
    union {
        VkImage img;
        VkBuffer buf;
    };
    VkDeviceMemory mems[VK_ALLOCATOR_MEMORY_PLANE_MAX];
    uint32_t mem_count;
    uint32_t mem_plane_count;

    bool coherent;
    bool protected;
};

struct vk_allocator_transfer {
    bool readback;
    bool writeback;
    VkBufferImageCopy copy;

    struct vk_buffer *staging;
};

static inline void
vk_allocator_init(struct vk_allocator *alloc, const char *render_node, bool protected)
{
    struct vk *vk = &alloc->vk;

    const char *dev_exts[32];
    uint32_t dev_ext_count = 0;

    if (render_node)
        dev_exts[dev_ext_count++] = VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME;

    /* VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT */
    dev_exts[dev_ext_count++] = VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME;
    dev_exts[dev_ext_count++] = VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME;

    /* VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT */
    dev_exts[dev_ext_count++] = VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME;
    dev_exts[dev_ext_count++] = VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME;

    /* to acquire/release ownership */
    dev_exts[dev_ext_count++] = VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME;

#if 0
    /* to disable metadata for front-rendering */
    dev_exts[dev_ext_count++] = VK_EXT_IMAGE_COMPRESSION_CONTROL_EXTENSION_NAME;

    /* to skip staging VkBuffer for transfers */
    dev_exts[dev_ext_count++] = VK_EXT_HOST_IMAGE_COPY_EXTENSION_NAME;
    dev_exts[dev_ext_count++] = VK_KHR_FORMAT_FEATURE_FLAGS2_EXTENSION_NAME;
    dev_exts[dev_ext_count++] = VK_KHR_COPY_COMMANDS2_EXTENSION_NAME;
#endif

    const struct vk_init_params params = {
        .render_node = render_node,
        .api_version = VK_API_VERSION_1_1,
        .protected_memory = protected,
        /* some of the exts can be dropped if we require 1.2 */
        .dev_exts = dev_exts,
        .dev_ext_count = dev_ext_count,
    };

    vk_init(vk, &params);

    alloc->handle_type = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
}

static inline void
vk_allocator_cleanup(struct vk_allocator *alloc)
{
    struct vk *vk = &alloc->vk;

    vk_cleanup(vk);
}

static inline uint32_t
vk_allocator_query_memory_type_mask(struct vk_allocator *alloc, VkMemoryPropertyFlags mem_flags)
{
    struct vk *vk = &alloc->vk;

    uint32_t mt_mask = 0;
    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        const VkMemoryType *mt = &vk->mem_props.memoryTypes[i];
        if ((mt->propertyFlags & mem_flags) == mem_flags)
            mt_mask |= 1 << i;
    }

    return mt_mask;
}

static inline bool
vk_allocator_is_external_memory_supported(struct vk_allocator *alloc,
                                          const VkExternalMemoryProperties *props)
{
    const VkExternalMemoryFeatureFlags required_feats =
        VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;
    if ((props->externalMemoryFeatures & required_feats) != required_feats)
        return false;

    if (!(props->exportFromImportedHandleTypes & alloc->handle_type))
        return false;

    if (!(props->compatibleHandleTypes & alloc->handle_type))
        return false;

    return true;
}

static inline bool
vk_allocator_query_buffer_support(struct vk_allocator *alloc,
                                  const struct vk_allocator_buffer_info *info)
{
    struct vk *vk = &alloc->vk;

    const VkPhysicalDeviceExternalBufferInfo external_info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO,
        .usage = info->usage,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkExternalBufferProperties external_props = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES,
    };
    vk->GetPhysicalDeviceExternalBufferProperties(vk->physical_dev, &external_info,
                                                  &external_props);

    return vk_allocator_is_external_memory_supported(alloc,
                                                     &external_props.externalMemoryProperties);
}

static inline uint64_t *
vk_allocator_query_format_modifiers(struct vk_allocator *alloc, VkFormat format, uint32_t *count)
{
    struct vk *vk = &alloc->vk;

    VkDrmFormatModifierPropertiesListEXT mod_list = {
        .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
    };
    VkFormatProperties2 fmt_props = {
        .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
        .pNext = &mod_list,
    };
    vk->GetPhysicalDeviceFormatProperties2(vk->physical_dev, format, &fmt_props);

    uint64_t *modifiers = NULL;
    uint32_t *mem_plane_counts;
    VkFormatFeatureFlags *format_features;
    VkDrmFormatModifierPropertiesEXT *mod_props;

    if (mod_list.drmFormatModifierCount) {
        const size_t size = (sizeof(*modifiers) + sizeof(*mem_plane_counts) +
                             sizeof(*format_features) + sizeof(*mod_props)) *
                            mod_list.drmFormatModifierCount;
        modifiers = malloc(size);
    }
    if (!modifiers) {
        *count = 0;
        return NULL;
    }

    mod_props = (void *)&modifiers[mod_list.drmFormatModifierCount * 2];
    mod_list.pDrmFormatModifierProperties = mod_props;
    vk->GetPhysicalDeviceFormatProperties2(vk->physical_dev, format, &fmt_props);

    mem_plane_counts = (void *)&modifiers[mod_list.drmFormatModifierCount];
    format_features = (void *)&mem_plane_counts[mod_list.drmFormatModifierCount];
    for (uint32_t i = 0; i < mod_list.drmFormatModifierCount; i++) {
        modifiers[i] = mod_props[i].drmFormatModifier;
        mem_plane_counts[i] = mod_props[i].drmFormatModifierPlaneCount;
        format_features[i] = mod_props[i].drmFormatModifierTilingFeatures;
    }

    *count = mod_list.drmFormatModifierCount;

    return modifiers;
}

static inline bool
vk_allocator_query_image_support(struct vk_allocator *alloc,
                                 const struct vk_allocator_image_info *info)
{
    struct vk *vk = &alloc->vk;

    /* too many planes for external image support */
    if (info->mem_plane_count > VK_ALLOCATOR_MEMORY_PLANE_MAX)
        return false;

    const VkImageCompressionControlEXT comp_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_CONTROL_EXT,
        .flags = info->compression,
    };
    const VkPhysicalDeviceImageDrmFormatModifierInfoEXT mod_info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
        .pNext = &comp_info,
        .drmFormatModifier = info->modifier,
    };
    const VkPhysicalDeviceExternalImageFormatInfo external_info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
        .pNext = &mod_info,
        .handleType = alloc->handle_type,
    };
    const VkPhysicalDeviceImageFormatInfo2 img_info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
        .pNext = &external_info,
        .format = info->format,
        .type = VK_IMAGE_TYPE_2D,
        .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
        .usage = info->usage,
        .flags = info->flags,
    };

    VkExternalImageFormatProperties external_props = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
    };
    VkImageFormatProperties2 img_props = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
        .pNext = &external_props,
    };
    vk->result =
        vk->GetPhysicalDeviceImageFormatProperties2(vk->physical_dev, &img_info, &img_props);
    if (vk->result != VK_SUCCESS)
        return false;

    return vk_allocator_is_external_memory_supported(alloc,
                                                     &external_props.externalMemoryProperties);
}

static inline void
vk_allocator_bo_destroy(struct vk_allocator *alloc, struct vk_allocator_bo *bo)
{
    struct vk *vk = &alloc->vk;

    for (uint32_t i = 0; i < bo->mem_count; i++)
        vk->FreeMemory(vk->dev, bo->mems[i], NULL);

    if (bo->is_img)
        vk->DestroyImage(vk->dev, bo->img, NULL);
    else
        vk->DestroyBuffer(vk->dev, bo->buf, NULL);

    free(bo);
}

static inline VkDeviceMemory
vk_allocator_bo_alloc_memory(struct vk_allocator *alloc,
                             struct vk_allocator_bo *bo,
                             const VkMemoryRequirements *reqs,
                             VkMemoryPropertyFlags mt_mask,
                             int import_fd)
{
    struct vk *vk = &alloc->vk;

    mt_mask &= reqs->memoryTypeBits;
    if (!mt_mask) {
        vk_log("no valid mt for resource");
        return false;
    }

    if (import_fd >= 0) {
        VkMemoryFdPropertiesKHR fd_props = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR,
        };
        vk->result =
            vk->GetMemoryFdPropertiesKHR(vk->dev, alloc->handle_type, import_fd, &fd_props);
        if (vk->result != VK_SUCCESS) {
            vk_log("invalid fd");
            return false;
        }

        mt_mask &= fd_props.memoryTypeBits;
        if (!mt_mask) {
            vk_log("no valid mt for fd");
            return false;
        }

        import_fd = dup(import_fd);
        if (import_fd < 0) {
            vk_log("failed to dup fd");
            return false;
        }
    }

    const VkImportMemoryFdInfoKHR import_info = {
        .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
        .handleType = alloc->handle_type,
        .fd = import_fd,
    };
    const VkExportMemoryAllocateInfo export_info = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        .pNext = import_fd >= 0 ? &import_info : NULL,
        .handleTypes = alloc->handle_type,
    };
    const VkMemoryDedicatedAllocateInfo dedicated_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        .pNext = &export_info,
        .image = bo->is_img ? bo->img : VK_NULL_HANDLE,
        .buffer = bo->is_img ? VK_NULL_HANDLE : bo->buf,
    };
    const VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        /* VUID-VkMemoryDedicatedAllocateInfo-image-01797 */
        .pNext = bo->mem_count > 1 ? dedicated_info.pNext : &dedicated_info,
        .allocationSize = reqs->size,
        .memoryTypeIndex = ffs(mt_mask) - 1,
    };
    VkDeviceMemory mem;
    vk->result = vk->AllocateMemory(vk->dev, &alloc_info, NULL, &mem);
    if (vk->result != VK_SUCCESS) {
        vk_log("failed to %s mem", import_fd >= 0 ? "import" : "alloc");
        close(import_fd);
        return VK_NULL_HANDLE;
    }

    return mem;
}

static inline struct vk_allocator_bo *
vk_allocator_bo_create_buffer(struct vk_allocator *alloc,
                              const struct vk_allocator_buffer_info *info,
                              VkDeviceSize size,
                              int import_fd)
{
    struct vk *vk = &alloc->vk;

    struct vk_allocator_bo *bo = calloc(1, sizeof(*bo));
    if (!bo)
        return NULL;

    bo->is_img = false;
    bo->mem_count = 1;
    bo->mem_plane_count = 1;
    bo->coherent = info->mt_coherent;
    bo->protected = info->flags & VK_BUFFER_CREATE_PROTECTED_BIT;

    const VkExternalMemoryBufferCreateInfo external_info = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
        .handleTypes = alloc->handle_type,
    };
    const VkBufferCreateInfo buf_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = &external_info,
        .flags = info->flags,
        .size = size,
        .usage = info->usage,
    };
    vk->result = vk->CreateBuffer(vk->dev, &buf_info, NULL, &bo->buf);
    if (vk->result != VK_SUCCESS)
        goto fail;

    const VkBufferMemoryRequirementsInfo2 reqs_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
        .buffer = bo->buf,
    };
    VkMemoryRequirements2 reqs = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
    };
    vk->GetBufferMemoryRequirements2(vk->dev, &reqs_info, &reqs);
    bo->mems[0] = vk_allocator_bo_alloc_memory(alloc, bo, &reqs.memoryRequirements, info->mt_mask,
                                               import_fd);
    if (bo->mems[0] == VK_NULL_HANDLE)
        goto fail;

    const VkBindBufferMemoryInfo bind_info = {
        .sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
        .buffer = bo->buf,
        .memory = bo->mems[0],
    };
    vk->result = vk->BindBufferMemory2(vk->dev, 1, &bind_info);
    if (vk->result != VK_SUCCESS) {
        vk_log("failed to bind mem");
        goto fail;
    }

    return bo;

fail:
    vk_allocator_bo_destroy(alloc, bo);
    return NULL;
}

static inline bool
vk_allocator_bo_align_image_layout(struct vk_allocator *alloc,
                                   struct vk_allocator_bo *bo,
                                   uint32_t offset_align,
                                   uint32_t pitch_align,
                                   VkSubresourceLayout *aligned_layouts)
{
    struct vk *vk = &alloc->vk;

    /* no need to check */
    if (offset_align == 1 && pitch_align == 1)
        return false;

    VkSubresourceLayout img_layouts[VK_ALLOCATOR_MEMORY_PLANE_MAX];
    uint32_t first_unaligned_plane = bo->mem_plane_count;
    uint32_t guessed_offset_align = 0;
    uint32_t offset_bits = 0;
    for (uint32_t i = 0; i < bo->mem_plane_count; i++) {
        const VkImageSubresource subres = {
            .aspectMask = VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT << i,
        };
        vk->GetImageSubresourceLayout(vk->dev, bo->img, &subres, &img_layouts[i]);

        aligned_layouts[i] = (VkSubresourceLayout){
            .offset = ALIGN(img_layouts[i].offset, offset_align),
            .size = 0,
            .rowPitch = ALIGN(img_layouts[i].rowPitch, pitch_align),
            .arrayPitch = 0,
            .depthPitch = 0,
        };
        if (aligned_layouts[i].offset != img_layouts[i].offset ||
            aligned_layouts[i].rowPitch != img_layouts[i].rowPitch) {
            if (first_unaligned_plane > i)
                first_unaligned_plane = i;

            if (aligned_layouts[i].offset != img_layouts[i].offset)
                guessed_offset_align = offset_align;
        }

        offset_bits |= img_layouts[i].offset;
    }

    /* already aligned */
    if (first_unaligned_plane >= bo->mem_plane_count)
        return false;

    /* If any plane other than the last one is changed, we have to fix the
     * offsets of all following planes.
     *
     * XXX None of these guess work would be needed if there was a vulkan
     * extension to express offset and pitch alignments.
     */
    if (first_unaligned_plane < bo->mem_plane_count - 1 && bo->mem_count == 1) {
        /* guess the offset align */
        if (!guessed_offset_align) {
            const uint32_t max_align = 4096;
            guessed_offset_align = offset_bits ? (1u << (ffs(offset_bits) - 1)) : max_align;
            if (guessed_offset_align > max_align)
                guessed_offset_align = max_align;
        }

        for (uint32_t i = first_unaligned_plane; i < bo->mem_plane_count - 1; i++) {
            /* guess the plane size */
            const uint32_t guessed_height =
                (img_layouts[i].size + img_layouts[i].rowPitch - 1) / img_layouts[i].rowPitch;
            const uint32_t guessed_size = aligned_layouts[i].rowPitch * guessed_height;

            const uint32_t guessed_offset = aligned_layouts[i].offset + guessed_size;
            aligned_layouts[i + 1].offset = ALIGN(guessed_offset, guessed_offset_align);
        }
    }

    for (uint32_t i = 0; i < bo->mem_plane_count; i++) {
        vk_log("adjust mem plane %d offset %d -> %d, pitch %d -> %d", i,
               (int)img_layouts[i].offset, (int)aligned_layouts[i].offset,
               (int)img_layouts[i].rowPitch, (int)aligned_layouts[i].rowPitch);
    }

    return true;
}

static inline struct vk_allocator_bo *
vk_allocator_bo_create_image(struct vk_allocator *alloc,
                             const struct vk_allocator_image_info *info,
                             uint32_t width,
                             uint32_t height,
                             uint32_t offset_align,
                             uint32_t pitch_align,
                             const int *import_fds)
{
    struct vk *vk = &alloc->vk;

    struct vk_allocator_bo *bo = calloc(1, sizeof(*bo));
    if (!bo)
        return NULL;

    bo->is_img = true;
    bo->mem_count = (info->flags & VK_IMAGE_CREATE_DISJOINT_BIT) ? info->mem_plane_count : 1;
    bo->mem_plane_count = info->mem_plane_count;
    bo->coherent = info->mt_coherent;
    bo->protected = info->flags & VK_IMAGE_CREATE_PROTECTED_BIT;

    const VkImageDrmFormatModifierListCreateInfoEXT mod_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT,
        .drmFormatModifierCount = 1,
        .pDrmFormatModifiers = &info->modifier,
    };
    VkImageCompressionControlEXT comp_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_CONTROL_EXT,
        .pNext = &mod_info,
        .flags = info->compression,
    };
    const VkExternalMemoryImageCreateInfo external_info = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .pNext = &comp_info,
        .handleTypes = alloc->handle_type,
    };
    const VkImageCreateInfo img_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &external_info,
        .flags = info->flags,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = info->format,
        .extent = {
            .width = width,
            .height = height,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
        .usage = info->usage,
    };
    vk->result = vk->CreateImage(vk->dev, &img_info, NULL, &bo->img);

    VkSubresourceLayout aligned_layouts[VK_ALLOCATOR_MEMORY_PLANE_MAX];
    if (vk->result == VK_SUCCESS && vk_allocator_bo_align_image_layout(
                                        alloc, bo, offset_align, pitch_align, aligned_layouts)) {
        vk->DestroyImage(vk->dev, bo->img, NULL);
        bo->img = VK_NULL_HANDLE;

        /* replace mod_info */
        assert(comp_info.pNext == &mod_info);
        const VkImageDrmFormatModifierExplicitCreateInfoEXT explicit_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT,
            .pNext = mod_info.pNext,
            .drmFormatModifier = info->modifier,
            .drmFormatModifierPlaneCount = info->mem_plane_count,
            .pPlaneLayouts = aligned_layouts,
        };
        comp_info.pNext = &explicit_info;
        vk->result = vk->CreateImage(vk->dev, &img_info, NULL, &bo->img);
    }

    if (vk->result != VK_SUCCESS)
        goto fail;

    for (uint32_t i = 0; i < bo->mem_count; i++) {
        const VkImagePlaneMemoryRequirementsInfo plane_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO,
            .planeAspect = VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT << i,
        };
        const VkImageMemoryRequirementsInfo2 reqs_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
            .pNext = bo->mem_count > 1 ? &plane_info : NULL,
            .image = bo->img,
        };
        VkMemoryDedicatedRequirements dedicated_reqs = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,
        };
        VkMemoryRequirements2 reqs = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
            .pNext = &dedicated_reqs,
        };
        vk->GetImageMemoryRequirements2(vk->dev, &reqs_info, &reqs);

        /* VUID-VkMemoryDedicatedAllocateInfo-image-01797
         * If image is not VK_NULL_HANDLE, image must not have been created
         * with VK_IMAGE_CREATE_DISJOINT_BIT set in VkImageCreateInfo::flags
         */
        if (dedicated_reqs.requiresDedicatedAllocation && bo->mem_count > 1)
            goto fail;

        const int import_fd = import_fds ? import_fds[i] : -1;
        bo->mems[i] = vk_allocator_bo_alloc_memory(alloc, bo, &reqs.memoryRequirements,
                                                   info->mt_mask, import_fd);
        if (bo->mems[i] == VK_NULL_HANDLE)
            goto fail;
    }

    VkBindImagePlaneMemoryInfo plane_infos[VK_ALLOCATOR_MEMORY_PLANE_MAX];
    VkBindImageMemoryInfo bind_infos[VK_ALLOCATOR_MEMORY_PLANE_MAX];
    for (uint32_t i = 0; i < bo->mem_count; i++) {
        plane_infos[i] = (VkBindImagePlaneMemoryInfo){
            .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO,
            .planeAspect = VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT << i,
        };
        bind_infos[i] = (VkBindImageMemoryInfo){
            .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
            .pNext = bo->mem_count > 1 ? &plane_infos[i] : NULL,
            .image = bo->img,
            .memory = bo->mems[i],
        };
    }
    vk->result = vk->BindImageMemory2(vk->dev, bo->mem_count, bind_infos);
    if (vk->result != VK_SUCCESS) {
        vk_log("failed to bind mem");
        goto fail;
    }

    return bo;

fail:
    vk_allocator_bo_destroy(alloc, bo);
    return NULL;
}

static inline void
vk_allocator_bo_query_layout(struct vk_allocator *alloc,
                             struct vk_allocator_bo *bo,
                             uint32_t *offsets,
                             uint32_t *pitches)
{
    struct vk *vk = &alloc->vk;

    if (!bo->is_img) {
        offsets[0] = 0;
        pitches[0] = 0;
        return;
    }

    for (uint32_t i = 0; i < bo->mem_plane_count; i++) {
        const VkImageSubresource subres = {
            .aspectMask = VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT << i,
        };
        VkSubresourceLayout layout;
        vk->GetImageSubresourceLayout(vk->dev, bo->img, &subres, &layout);

        offsets[i] = layout.offset;
        pitches[i] = layout.rowPitch;
    }
}

static inline bool
vk_allocator_bo_export_fds(struct vk_allocator *alloc, struct vk_allocator_bo *bo, int *fds)
{
    struct vk *vk = &alloc->vk;

    for (uint32_t i = 0; i < bo->mem_count; i++) {
        const VkMemoryGetFdInfoKHR info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
            .memory = bo->mems[i],
            .handleType = alloc->handle_type,
        };
        vk->result = vk->GetMemoryFdKHR(vk->dev, &info, &fds[i]);
        if (vk->result != VK_SUCCESS) {
            for (uint32_t j = 0; j < i; j++) {
                close(fds[j]);
                fds[j] = -1;
            }
            break;
        }
    }

    return vk->result == VK_SUCCESS;
}

static inline void *
vk_allocator_bo_map(struct vk_allocator *alloc, struct vk_allocator_bo *bo, uint32_t mem_plane)
{
    struct vk *vk = &alloc->vk;

    void *ptr;
    vk->result = vk->MapMemory(vk->dev, bo->mems[mem_plane], 0, VK_WHOLE_SIZE, 0, &ptr);
    if (vk->result != VK_SUCCESS)
        return NULL;

    if (!bo->coherent) {
        const VkMappedMemoryRange range = {
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = bo->mems[mem_plane],
            .size = VK_WHOLE_SIZE,
        };
        vk->result = vk->InvalidateMappedMemoryRanges(vk->dev, 1, &range);
        if (vk->result != VK_SUCCESS) {
            vk->UnmapMemory(vk->dev, bo->mems[mem_plane]);
            return NULL;
        }
    }

    return ptr;
}

static inline void
vk_allocator_bo_unmap(struct vk_allocator *alloc, struct vk_allocator_bo *bo, uint32_t mem_plane)
{
    struct vk *vk = &alloc->vk;

    if (!bo->coherent) {
        const VkMappedMemoryRange range = {
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = bo->mems[mem_plane],
            .size = VK_WHOLE_SIZE,
        };
        vk->result = vk->FlushMappedMemoryRanges(vk->dev, 1, &range);
        if (vk->result != VK_SUCCESS)
            vk_log("failed to flush mapped memory");
    }

    vk->UnmapMemory(vk->dev, bo->mems[mem_plane]);
}

static inline struct vk_allocator_transfer *
vk_allocator_bo_map_transfer(struct vk_allocator *alloc,
                             struct vk_allocator_bo *bo,
                             VkBufferUsageFlags usage,
                             VkImageAspectFlagBits aspect,
                             uint32_t x,
                             uint32_t y,
                             uint32_t width,
                             uint32_t height)
{
    struct vk *vk = &alloc->vk;

    if (!bo->is_img)
        return NULL;

    struct vk_allocator_transfer *xfer = calloc(1, sizeof(*xfer));
    if (!xfer)
        return NULL;

    xfer->readback = usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    xfer->writeback = usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    xfer->copy = (VkBufferImageCopy){
        .imageSubresource = {
            .aspectMask = aspect,
            .layerCount = 1,
        },
        .imageOffset = {
            .x = x,
            .y = y,
        },
        .imageExtent = {
            .width = width,
            .height = height,
            .depth = 1,
        },
    };

    const uint32_t bpp = 32;
    VkDeviceSize size = width * height * bpp;
    xfer->staging =
        vk_create_buffer(vk, bo->protected ? VK_BUFFER_CREATE_PROTECTED_BIT : 0, size, usage);

    if (xfer->readback) {
        VkCommandBuffer cmd = vk_begin_cmd(vk, bo->protected);

        /* assume the foreign queue has transitioned the image to
         * VK_IMAGE_LAYOUT_GENERAL
         */
        const VkImageMemoryBarrier img_acquire = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
            .dstQueueFamilyIndex = vk->queue_family_index,
            .image = bo->img,
            .subresourceRange = {
                .aspectMask = xfer->copy.imageSubresource.aspectMask,
                .levelCount = 1,
                .layerCount = xfer->copy.imageSubresource.layerCount,
            },
        };
        vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
                               &img_acquire);

        vk->CmdCopyImageToBuffer(cmd, bo->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 xfer->staging->buf, 1, &xfer->copy);

        const VkBufferMemoryBarrier buf_barrier = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask =
                VK_ACCESS_HOST_READ_BIT | (xfer->writeback ? VK_ACCESS_HOST_WRITE_BIT : 0),
            .buffer = xfer->staging->buf,
            .size = VK_WHOLE_SIZE,
        };
        vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
                               0, NULL, 1, &buf_barrier, 0, NULL);

        vk_end_cmd(vk);
        vk_wait(vk);
    }

    return xfer;
}

static inline void
vk_allocator_bo_unmap_transfer(struct vk_allocator *alloc,
                               struct vk_allocator_bo *bo,
                               struct vk_allocator_transfer *xfer)
{
    struct vk *vk = &alloc->vk;

    if (xfer->writeback) {
        VkCommandBuffer cmd = vk_begin_cmd(vk, bo->protected);

        if (xfer->readback) {
            const VkImageMemoryBarrier img_barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_NONE,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .image = bo->img,
                .subresourceRange = {
                    .aspectMask = xfer->copy.imageSubresource.aspectMask,
                    .levelCount = 1,
                    .layerCount = xfer->copy.imageSubresource.layerCount,
                },
            };
            vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                   VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
                                   &img_barrier);
        } else {
            const VkImageMemoryBarrier img_acquire = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
                .dstQueueFamilyIndex = vk->queue_family_index,
                .image = bo->img,
                .subresourceRange = {
                    .aspectMask = xfer->copy.imageSubresource.aspectMask,
                    .levelCount = 1,
                    .layerCount = xfer->copy.imageSubresource.layerCount,
                },
            };
            vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
                                   &img_acquire);
        }

        const VkBufferMemoryBarrier buf_barrier = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .buffer = xfer->staging->buf,
            .size = VK_WHOLE_SIZE,
        };
        vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                               0, NULL, 1, &buf_barrier, 0, NULL);

        vk->CmdCopyBufferToImage(cmd, xfer->staging->buf, bo->img,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &xfer->copy);

        const VkImageMemoryBarrier img_release = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = vk->queue_family_index,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
            .image = bo->img,
            .subresourceRange = {
                .aspectMask = xfer->copy.imageSubresource.aspectMask,
                .levelCount = 1,
                .layerCount = xfer->copy.imageSubresource.layerCount,
            },
        };
        vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1,
                               &img_release);

        vk_end_cmd(vk);
        vk_wait(vk);
    }

    vk_destroy_buffer(vk, xfer->staging);
    free(xfer);
}

#endif /* VKUTIL_ALLOCATOR_H */
