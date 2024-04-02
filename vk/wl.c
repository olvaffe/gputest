/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil_allocator.h"
#include "wlutil.h"

struct wl_test {
    uint32_t width;
    uint32_t height;
    VkFormat vk_format;
    uint32_t drm_format;
    uint64_t modifier;
    bool shm;

    struct wl wl;
    struct vk_allocator alloc;

    struct wl_swapchain *swapchain;
    bool quit;
};

static void
wl_test_dispatch_key(void *data, uint32_t key)
{
    struct wl_test *test = data;

    switch (key) {
    case KEY_ESC:
    case KEY_Q:
        test->quit = true;
        break;
    default:
        break;
    }
}

static void
wl_test_dispatch_close(void *data)
{
    struct wl_test *test = data;

    test->quit = true;
}

static void
wl_test_paint_yuv_pattern(struct wl_test *test, void *dst, uint32_t pitch, uint32_t plane)
{
    for (uint32_t y = 0; y < test->height; y++) {
        const float v = (float)y / (test->height - 1);
        const uint8_t rgb[3] = {
            (int)((1.0f - v) * 255),
            (int)(0.1f * 255),
            (int)(v * 255),
        };
        uint8_t yuv[3];
        vk_rgb_to_yuv(rgb, yuv);

        union {
            void *ptr;
            uint8_t *u8;
            uint16_t *u16;
        } row;
        row.ptr = dst + y * pitch;

        union {
            uint8_t u8;
            uint16_t u16;
        } packed;

        if (plane == 0) {
            packed.u8 = yuv[0];
            memset(row.u8, packed.u8, test->width);
            continue;
        }

        /* 420 subsampling */
        if (y & 1)
            continue;
        row.ptr = dst + y / 2 * pitch;
        for (uint32_t x = 0; x < test->width / 2; x++) {
            switch (test->drm_format) {
            case DRM_FORMAT_YVU420:
                packed.u8 = yuv[3 - plane];
                row.u8[x] = packed.u8;
                break;
            case DRM_FORMAT_NV12:
                packed.u16 = yuv[1] | yuv[2] << 8;
                row.u16[x] = packed.u16;
                break;
            default:
                wl_die("unsupported planar format");
                break;
            }
        }
    }
}

static void
wl_test_paint_rgba_pattern(struct wl_test *test, void *dst, uint32_t pitch)
{
    for (uint32_t y = 0; y < test->height; y++) {
        const float v = (float)y / (test->height - 1);
        const float rgba[4] = {
            1.0f - v,
            0.1f,
            v,
            0.3f,
        };

        union {
            void *ptr;
            uint16_t *u16;
            uint32_t *u32;
        } row;
        row.ptr = dst + y * pitch;

        union {
            uint16_t u16;
            uint32_t u32;
        } packed;

        for (uint32_t x = 0; x < test->width; x++) {
            switch (test->drm_format) {
            case DRM_FORMAT_ARGB8888:
            case DRM_FORMAT_XRGB8888:
                packed.u32 = (int)(rgba[0] * 255) << 16 | (int)(rgba[1] * 255) << 8 |
                             (int)(rgba[2] * 255) << 0 | (int)(rgba[3] * 255) << 24;
                row.u32[x] = packed.u32;
                break;
            case DRM_FORMAT_ABGR8888:
            case DRM_FORMAT_XBGR8888:
                packed.u32 = (int)(rgba[0] * 255) << 0 | (int)(rgba[1] * 255) << 8 |
                             (int)(rgba[2] * 255) << 16 | (int)(rgba[3] * 255) << 24;
                row.u32[x] = packed.u32;
                break;
            case DRM_FORMAT_RGB565:
                packed.u16 = (int)(rgba[0] * 31) << 11 | (int)(rgba[1] * 63) << 5 |
                             (int)(rgba[2] * 31) << 0;
                row.u16[x] = packed.u16;
                break;
            default:
                wl_die("unsupported format");
                break;
            }
        }
    }
}

static void
wl_test_dispatch_redraw(void *data)
{
    struct wl_test *test = data;
    struct wl *wl = &test->wl;
    struct vk_allocator *alloc = &test->alloc;

    const struct wl_swapchain_image *img = wl_acquire_swapchain_image(wl, test->swapchain);

    if (test->shm) {
        const uint32_t pitch = test->width * wl_drm_format_cpp(test->drm_format);
        wl_test_paint_rgba_pattern(test, img->data, pitch);
    } else if (test->modifier == DRM_FORMAT_MOD_LINEAR) {
        struct vk_allocator_bo *bo = img->data;
        void *ptr = vk_allocator_bo_map(alloc, bo, 0);

        uint32_t offsets[VK_ALLOCATOR_MEMORY_PLANE_MAX];
        uint32_t pitches[VK_ALLOCATOR_MEMORY_PLANE_MAX];
        vk_allocator_bo_query_layout(alloc, bo, offsets, pitches);

        assert(bo->mem_plane_count == wl_drm_format_plane_count(test->drm_format));
        for (uint32_t plane = 0; plane < bo->mem_plane_count; plane++) {
            if (bo->mem_plane_count > 1)
                wl_test_paint_yuv_pattern(test, ptr + offsets[plane], pitches[plane], plane);
            else
                wl_test_paint_rgba_pattern(test, ptr + offsets[plane], pitches[plane]);
        }

        vk_allocator_bo_unmap(alloc, bo, 0);
    } else {
        struct vk_allocator_bo *bo = img->data;
        if (bo->mem_plane_count == 1) {
            const uint32_t pitch = test->width * wl_drm_format_cpp(test->drm_format);
            struct vk_allocator_bo *bo = img->data;
            struct vk_allocator_transfer *xfer = vk_allocator_bo_map_transfer(
                alloc, bo, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0, 0,
                test->width, test->height);

            wl_test_paint_rgba_pattern(test, xfer->staging->mem_ptr, pitch);

            vk_allocator_bo_unmap_transfer(alloc, bo, xfer);
        } else {
            struct vk_allocator_bo *bo = img->data;

            if (bo->mem_plane_count != wl_drm_format_plane_count(test->drm_format))
                wl_die("no aux plane support");

            for (uint32_t plane = 0; plane < bo->mem_plane_count; plane++) {
                const VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_PLANE_0_BIT << plane;
                uint32_t width = test->width;
                uint32_t height = test->height;
                uint32_t pitch = test->width;

                /* 420 subsampling */
                if (plane > 0) {
                    width /= 2;
                    height /= 2;

                    if (bo->mem_plane_count == 3)
                        pitch /= 2;
                }

                struct vk_allocator_transfer *xfer = vk_allocator_bo_map_transfer(
                    alloc, bo, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, aspect, 0, 0, width, height);

                wl_test_paint_yuv_pattern(test, xfer->staging->mem_ptr, pitch, plane);

                vk_allocator_bo_unmap_transfer(alloc, bo, xfer);
            }
        }
    }

    wl_present_swapchain_image(wl, test->swapchain, img);
}

static void
wl_test_loop(struct wl_test *test)
{
    struct wl *wl = &test->wl;

    if (wl->xdg_ready)
        wl_test_dispatch_redraw(test);

    while (!test->quit)
        wl_dispatch(wl);
}

static void
wl_test_init_swapchain(struct wl_test *test)
{
    const uint32_t image_count = 3;
    struct wl *wl = &test->wl;
    struct vk_allocator *alloc = &test->alloc;

    test->swapchain = wl_create_swapchain(wl, test->width, test->height, test->drm_format,
                                          test->modifier, image_count);

    if (test->shm) {
        wl_add_swapchain_images_shm(wl, test->swapchain);
        return;
    }

    uint32_t mod_count;
    uint64_t *modifiers = vk_allocator_query_format_modifiers(alloc, test->vk_format, &mod_count);
    uint32_t *mem_plane_counts = (void *)&modifiers[mod_count];

    uint32_t mem_plane_count = 0;
    for (uint32_t i = 0; i < mod_count; i++) {
        if (modifiers[i] == test->modifier) {
            mem_plane_count = mem_plane_counts[i];
            break;
        }
    }
    if (!mem_plane_count)
        vk_die("unsupported modifier");

    const VkMemoryPropertyFlags mt_flags =
        test->modifier == DRM_FORMAT_MOD_LINEAR
            ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
            : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    const VkImageUsageFlags img_usage =
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    const struct vk_allocator_image_info img_info = {
        .format = test->vk_format,
        .modifier = test->modifier,
        .mem_plane_count = mem_plane_count,
        .usage = img_usage,
        .mt_mask = vk_allocator_query_memory_type_mask(alloc, mt_flags),
        .mt_coherent = mt_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };
    if (!vk_allocator_query_image_support(alloc, &img_info))
        vk_die("unsupported image");

    for (uint32_t i = 0; i < image_count; i++) {
        struct wl_swapchain_image *img = &test->swapchain->images[i];
        struct vk_allocator_bo *bo =
            vk_allocator_bo_create_image(alloc, &img_info, test->width, test->height, 1, 1, NULL);

        int fd;
        uint32_t offsets[VK_ALLOCATOR_MEMORY_PLANE_MAX];
        uint32_t pitches[VK_ALLOCATOR_MEMORY_PLANE_MAX];

        /* non-disjoint */
        assert(bo->mem_count == 1);
        if (!vk_allocator_bo_export_fds(alloc, bo, &fd))
            vk_die("failed to export bo");
        vk_allocator_bo_query_layout(alloc, bo, offsets, pitches);

        wl_add_swapchain_image_dmabuf(wl, test->swapchain, img, fd, offsets, pitches,
                                      bo->mem_plane_count);
        img->data = bo;

        close(fd);
    }
}

static void
wl_test_init(struct wl_test *test)
{
    struct wl *wl = &test->wl;
    struct vk_allocator *alloc = &test->alloc;

    const struct wl_init_params wl_params = {
        .data = test,
        .close = wl_test_dispatch_close,
        .redraw = wl_test_dispatch_redraw,
        .key = wl_test_dispatch_key,
    };
    wl_init(wl, &wl_params);
    wl_info(wl);

    /* TODO use wl->active.{main_dev,target_dev} */
    vk_allocator_init(alloc, NULL, false);

    wl_test_init_swapchain(test);
}

static void
wl_test_cleanup(struct wl_test *test)
{
    struct wl *wl = &test->wl;
    struct vk_allocator *alloc = &test->alloc;

    if (!test->shm) {
        for (uint32_t i = 0; i < test->swapchain->image_count; i++) {
            struct wl_swapchain_image *img = &test->swapchain->images[i];
            vk_allocator_bo_destroy(alloc, img->data);
        }
    }
    wl_destroy_swapchain(wl, test->swapchain);

    vk_allocator_cleanup(alloc);
    wl_cleanup(wl);
}

int
main(void)
{
    struct wl_test test = {
        .width = 320,
        .height = 240,
        .vk_format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
        .drm_format = DRM_FORMAT_NV12,
        .modifier = DRM_FORMAT_MOD_LINEAR,
        .shm = false,
    };

    wl_test_init(&test);
    wl_test_loop(&test);
    wl_test_cleanup(&test);

    return 0;
}
