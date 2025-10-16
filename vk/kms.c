/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "dmautil.h"
#include "drmutil.h"
#include "gbmutil.h"
#include "vkutil.h"

struct kms_test {
    int drm_index;
    const char *gbm_path;
    uint32_t drm_format;
    VkFormat vk_format;
    VkExternalMemoryHandleTypeFlagBits handle_type;
    bool protected;

    struct drm drm;
    struct vk vk;

    const struct drm_crtc *crtc;
    const struct drm_plane *plane;
    const struct drm_connector *connector;
    const struct drm_mode_modeinfo *mode;
    bool plane_active;

    struct gbm_import_fd_modifier_data bo;
    uint32_t fb_id;
    VkImage img;
    VkDeviceMemory mem;
};

static void
kms_test_init_memory(struct kms_test *test)
{
    struct vk *vk = &test->vk;

    VkMemoryRequirements reqs;
    vk->GetImageMemoryRequirements(vk->dev, test->img, &reqs);

    const int fd = dup(test->bo.fds[0]);
    if (fd < 0)
        vk_die("failed to dup dma-buf");

    VkMemoryFdPropertiesKHR fd_props = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR,
    };
    vk->result = vk->GetMemoryFdPropertiesKHR(vk->dev, test->handle_type, fd, &fd_props);
    vk_check(vk, "invalid dma-buf");

    uint32_t mt_mask = reqs.memoryTypeBits & fd_props.memoryTypeBits;
    if (!mt_mask)
        vk_die("no valid mt");

    const uint32_t mt = ffs(mt_mask) - 1;

    const VkImportMemoryFdInfoKHR import_info = {
        .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
        .handleType = test->handle_type,
        .fd = fd,
    };
    const VkMemoryDedicatedAllocateInfo dedicated_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        .pNext = &import_info,
        .image = test->img,
    };
    const VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &dedicated_info,
        .allocationSize = reqs.size,
        .memoryTypeIndex = mt,
    };
    vk->result = vk->AllocateMemory(vk->dev, &alloc_info, NULL, &test->mem);
    vk_check(vk, "failed to import dma-buf");

    vk->result = vk->BindImageMemory(vk->dev, test->img, test->mem, 0);
    vk_check(vk, "failed to bind image memory");
}

static void
kms_test_init_image(struct kms_test *test)
{
    struct vk *vk = &test->vk;

    VkSubresourceLayout explicit_layouts[GBM_MAX_PLANES] = { 0 };
    for (uint32_t i = 0; i < test->bo.num_fds; i++) {
        explicit_layouts[i].offset = test->bo.offsets[i];
        explicit_layouts[i].rowPitch = test->bo.strides[i];
    }

    const VkImageDrmFormatModifierExplicitCreateInfoEXT explicit_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT,
        .drmFormatModifier = test->bo.modifier,
        .drmFormatModifierPlaneCount = test->bo.num_fds,
        .pPlaneLayouts = explicit_layouts,
    };
    const VkExternalMemoryImageCreateInfo external_info = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .pNext = &explicit_info,
        .handleTypes = test->handle_type,
    };
    const VkImageCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &external_info,
        .flags = test->protected ? VK_IMAGE_CREATE_PROTECTED_BIT : 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = test->vk_format,
        .extent = {
            .width = test->bo.width,
            .height = test->bo.height,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    vk->result = vk->CreateImage(vk->dev, &info, NULL, &test->img);
    vk_check(vk, "failed to create image");
}

static void
kms_test_init_req(struct kms_test *test)
{
    struct drm *drm = &test->drm;

    drm_reset_req(drm);
    drm_add_property(drm, test->plane->id, test->plane->properties, "FB_ID", test->fb_id);

    if (!test->plane_active) {
        drm_add_property(drm, test->plane->id, test->plane->properties, "CRTC_ID",
                         test->crtc->id);
        drm_add_property(drm, test->plane->id, test->plane->properties, "SRC_X", 0);
        drm_add_property(drm, test->plane->id, test->plane->properties, "SRC_Y", 0);
        drm_add_property(drm, test->plane->id, test->plane->properties, "SRC_W",
                         test->bo.width << 16);
        drm_add_property(drm, test->plane->id, test->plane->properties, "SRC_H",
                         test->bo.height << 16);
        drm_add_property(drm, test->plane->id, test->plane->properties, "CRTC_X", 0);
        drm_add_property(drm, test->plane->id, test->plane->properties, "CRTC_Y", 0);
        drm_add_property(drm, test->plane->id, test->plane->properties, "CRTC_W", test->bo.width);
        drm_add_property(drm, test->plane->id, test->plane->properties, "CRTC_H",
                         test->bo.height);
    }
}

static void
kms_test_init_fb(struct kms_test *test)
{
    struct drm *drm = &test->drm;

    uint32_t handles[4];
    uint32_t pitches[4];
    uint32_t offsets[4];
    for (uint32_t i = 0; i < test->bo.num_fds; i++) {
        handles[i] = drm_prime_import(drm, test->bo.fds[i]);
        pitches[i] = test->bo.strides[i];
        offsets[i] = test->bo.offsets[i];
    }

    if (drmModeAddFB2WithModifiers(drm->fd, test->bo.width, test->bo.height, test->bo.format,
                                   handles, pitches, offsets, NULL, &test->fb_id, 0))
        drm_die("failed to create fb");

    for (uint32_t i = 0; i < test->bo.num_fds; i++)
        drmCloseBufferHandle(drm->fd, handles[i]);
}

static void
kms_test_init_bo(struct kms_test *test)
{
    struct gbm local_gbm;
    struct gbm *gbm = &local_gbm;

    const struct gbm_init_params gbm_params = {
        .path = test->gbm_path,
    };
    gbm_init(gbm, &gbm_params);

    const uint64_t mod = DRM_FORMAT_MOD_LINEAR;
    uint32_t flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;
    if (test->protected)
        flags |= GBM_BO_USE_PROTECTED;

    struct gbm_bo *bo = gbm_create_bo(gbm, test->mode->hdisplay, test->mode->vdisplay,
                                      test->drm_format, &mod, 1, flags);

    gbm_export_bo(gbm, bo, &test->bo);

    gbm_destroy_bo(gbm, bo);

    gbm_cleanup(gbm);
}

static void
kms_test_init_pipe(struct kms_test *test)
{
    struct drm *drm = &test->drm;

    for (uint32_t i = 0; i < drm->modeset.connector_count; i++) {
        const struct drm_connector *connector = &drm->modeset.connectors[i];

        /* use the first active connector */
        if (connector->crtc_id && connector->connected) {
            test->connector = connector;
            break;
        }
    }
    if (!test->connector)
        vk_die("no active connector");

    uint32_t crtc_idx;
    for (uint32_t i = 0; i < drm->modeset.crtc_count; i++) {
        const struct drm_crtc *crtc = &drm->modeset.crtcs[i];

        /* use the active crtc */
        if (crtc->id == test->connector->crtc_id) {
            test->crtc = crtc;
            crtc_idx = i;
            break;
        }
    }
    if (!test->crtc)
        vk_die("no active crtc");

    /* use the active mode */
    if (test->crtc->mode_valid)
        test->mode = &test->crtc->mode;
    else
        vk_die("no valid mode");

    for (uint32_t i = 0; i < drm->modeset.plane_count; i++) {
        const struct drm_plane *plane = &drm->modeset.planes[i];

        /* use the active plane */
        if (plane->crtc_id == test->crtc->id) {
            test->plane = plane;
            test->plane_active = true;
            break;
        }
    }
    if (!test->plane) {
        for (uint32_t i = 0; i < drm->modeset.plane_count; i++) {
            const struct drm_plane *plane = &drm->modeset.planes[i];

            /* use the first possible plane */
            if (plane->possible_crtcs & (1 << crtc_idx)) {
                test->plane = plane;
                test->plane_active = false;
                break;
            }
        }
    }
    if (!test->plane) {
        vk_die("no plane");
    }

    bool has_format = false;
    for (uint32_t i = 0; i < test->plane->format_count; i++) {
        if (test->plane->formats[i] == test->drm_format) {
            has_format = true;
            break;
        }
    }
    if (!has_format)
        vk_die("no format");
}

static void
kms_test_init(struct kms_test *test)
{
    struct drm *drm = &test->drm;
    struct vk *vk = &test->vk;

    drm_init(drm, NULL);
    drm_open(drm, test->drm_index, DRM_NODE_PRIMARY);
    drm_scan_resources(drm);

    const char *const dev_exts[] = {
        VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
        VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
        VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
    };
    const struct vk_init_params vk_params = {
        .protected_memory = test->protected,
        .dev_exts = dev_exts,
        .dev_ext_count = ARRAY_SIZE(dev_exts),
    };
    vk_init(vk, &vk_params);

    kms_test_init_pipe(test);
    kms_test_init_bo(test);
    kms_test_init_fb(test);
    kms_test_init_req(test);
    kms_test_init_image(test);
    kms_test_init_memory(test);
}

static void
kms_test_cleanup(struct kms_test *test)
{
    struct drm *drm = &test->drm;
    struct vk *vk = &test->vk;

    vk->FreeMemory(vk->dev, test->mem, NULL);
    vk->DestroyImage(vk->dev, test->img, NULL);

    drmModeRmFB(drm->fd, test->fb_id);

    for (uint32_t i = 0; i < test->bo.num_fds; i++)
        close(test->bo.fds[i]);

    vk_cleanup(vk);

    drm_release_resources(drm);
    drm_close(drm);
    drm_cleanup(drm);
}

static void
kms_test_draw(struct kms_test *test)
{
    struct drm *drm = &test->drm;
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, test->protected);

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier barrier1 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
        .dstQueueFamilyIndex = vk->queue_family_index,
        .image = test->img,
        .subresourceRange = subres_range,
    };
    const VkImageMemoryBarrier barrier2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = vk->queue_family_index,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
        .image = test->img,
        .subresourceRange = subres_range,
    };

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0, 0, NULL, 0, NULL, 1, &barrier1);

    const VkClearColorValue clear_val = {
        .float32 = { 1.0f, 0.5f, 0.5f, 1.0f },
    };

    vk->CmdClearColorImage(cmd, test->img, barrier1.newLayout, &clear_val, 1, &subres_range);

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1,
                           &barrier2);

    vk_end_cmd(vk);
    vk_wait(vk);

    drm_commit(drm);
    u_sleep(1000);
}

int
main(int argc, char **argv)
{
    struct kms_test test = {
        .drm_index = 0,
        .gbm_path = "/dev/dri/renderD128",
        .drm_format = DRM_FORMAT_XRGB8888,
        .vk_format = VK_FORMAT_B8G8R8A8_SRGB,
        .handle_type = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
        .protected = false,
    };

    kms_test_init(&test);
    kms_test_draw(&test);
    kms_test_cleanup(&test);

    return 0;
}
