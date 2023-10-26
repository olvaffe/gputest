/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil_allocator.h"

struct external_test_format {
    uint32_t fourcc;
    VkFormat format;

    uint32_t subsampling;
    uint32_t plane_count;
    struct {
        VkImageAspectFlagBits aspect;
        uint32_t bpp;
    } planes[3];
};

#define EXTERNAL_TEST_LITTLE_ENDIAN 1
static const struct external_test_format external_test_formats[] = {
    /* sub-byte components */
    {
        .fourcc = DRM_FORMAT_BGR565,
#ifdef EXTERNAL_TEST_LITTLE_ENDIAN
        .format = VK_FORMAT_B5G6R5_UNORM_PACK16,
#else
	.format = VK_FORMAT_R5G6B5_UNORM_PACK16,
#endif
        .plane_count = 1,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .bpp = 16,
        },
    },
    {
        .fourcc = DRM_FORMAT_RGB565,
#ifdef EXTERNAL_TEST_LITTLE_ENDIAN
        .format = VK_FORMAT_R5G6B5_UNORM_PACK16,
#else
	.format = VK_FORMAT_B5G6R5_UNORM_PACK16,
#endif
        .plane_count = 1,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .bpp = 16,
        },
    },
    /* 1-3 byte-sized components */
    {
        .fourcc = DRM_FORMAT_R8,
        .format = VK_FORMAT_R8_UNORM,
        .plane_count = 1,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .bpp = 8,
        },
    },
    {
        .fourcc = DRM_FORMAT_GR88,
        .format = VK_FORMAT_R8G8_UNORM,
        .plane_count = 1,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .bpp = 16,
        },
    },
    {
        .fourcc = DRM_FORMAT_BGR888,
        .format = VK_FORMAT_R8G8B8_UNORM,
        .plane_count = 1,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .bpp = 24,
        },
    },
    {
        .fourcc = DRM_FORMAT_RGB888,
        .format = VK_FORMAT_B8G8R8_UNORM,
        .plane_count = 1,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .bpp = 24,
        },
    },
    /* 4 byte-sized components */
    {
        .fourcc = DRM_FORMAT_ABGR8888,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .plane_count = 1,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .bpp = 32,
        },
    },
    {
        .fourcc = DRM_FORMAT_XBGR8888,
        .format = VK_FORMAT_UNDEFINED,
        .plane_count = 1,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .bpp = 32,
        },
    },
    {
        .fourcc = DRM_FORMAT_ARGB8888,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .plane_count = 1,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .bpp = 32,
        },
    },
    {
        .fourcc = DRM_FORMAT_XRGB8888,
        .format = VK_FORMAT_UNDEFINED,
        .plane_count = 1,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .bpp = 32,
        },
    },
    /* 10-bit components */
    {
        .fourcc = DRM_FORMAT_ABGR2101010,
#ifdef EXTERNAL_TEST_LITTLE_ENDIAN
        .format = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
#else
        .format = VK_FORMAT_UNDEFINED,
#endif
        .plane_count = 1,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .bpp = 32,
        },
    },
    {
        .fourcc = DRM_FORMAT_XBGR2101010,
        .format = VK_FORMAT_UNDEFINED,
        .plane_count = 1,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .bpp = 32,
        },
    },
    {
        .fourcc = DRM_FORMAT_ARGB2101010,
#ifdef EXTERNAL_TEST_LITTLE_ENDIAN
        .format = VK_FORMAT_A2R10G10B10_UNORM_PACK32,
#else
        .format = VK_FORMAT_UNDEFINED,
#endif
        .plane_count = 1,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .bpp = 32,
        },
    },
    {
        .fourcc = DRM_FORMAT_XRGB2101010,
        .format = VK_FORMAT_UNDEFINED,
        .plane_count = 1,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .bpp = 32,
        },
    },
    /* 16-bit components */
    {
        .fourcc = DRM_FORMAT_R16,
        .format = VK_FORMAT_R16_UNORM,
        .plane_count = 1,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .bpp = 16,
        },
    },
    {
        .fourcc = DRM_FORMAT_ABGR16161616F,
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .plane_count = 1,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .bpp = 64,
        },
    },
    /* Y'CbCr */
    {
        .fourcc = DRM_FORMAT_YUYV,
        .format = VK_FORMAT_G8B8G8R8_422_UNORM,
        .subsampling = 422,
        .plane_count = 1,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .bpp = 32,
        },
    },
    {
        .fourcc = DRM_FORMAT_UYVY,
        .format = VK_FORMAT_B8G8R8G8_422_UNORM,
        .subsampling = 422,
        .plane_count = 1,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .bpp = 32,
        },
    },
    {
        .fourcc = DRM_FORMAT_NV12,
        .format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
        .subsampling = 420,
        .plane_count = 2,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_PLANE_0_BIT,
            .bpp = 8,
        },
        .planes[1] = {
            .aspect = VK_IMAGE_ASPECT_PLANE_1_BIT,
            .bpp = 16,
        },
    },
    {
        .fourcc = DRM_FORMAT_NV21,
        .format = VK_FORMAT_UNDEFINED,
        .subsampling = 420,
        .plane_count = 2,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_PLANE_0_BIT,
            .bpp = 8,
        },
        .planes[1] = {
            .aspect = VK_IMAGE_ASPECT_PLANE_1_BIT,
            .bpp = 16,
        },
    },
    {
        .fourcc = DRM_FORMAT_YUV420,
        .format = VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
        .subsampling = 420,
        .plane_count = 3,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_PLANE_0_BIT,
            .bpp = 8,
        },
        .planes[1] = {
            .aspect = VK_IMAGE_ASPECT_PLANE_1_BIT,
            .bpp = 8,
        },
        .planes[2] = {
            .aspect = VK_IMAGE_ASPECT_PLANE_2_BIT,
            .bpp = 8,
        },
    },
    {
        .fourcc = DRM_FORMAT_YVU420,
        .format = VK_FORMAT_UNDEFINED,
        .subsampling = 420,
        .plane_count = 3,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_PLANE_0_BIT,
            .bpp = 8,
        },
        .planes[1] = {
            .aspect = VK_IMAGE_ASPECT_PLANE_1_BIT,
            .bpp = 8,
        },
        .planes[2] = {
            .aspect = VK_IMAGE_ASPECT_PLANE_2_BIT,
            .bpp = 8,
        },
    },
    {
        .fourcc = DRM_FORMAT_P010,
        .format = VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
        .subsampling = 420,
        .plane_count = 2,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_PLANE_0_BIT,
            .bpp = 16,
        },
        .planes[1] = {
            .aspect = VK_IMAGE_ASPECT_PLANE_1_BIT,
            .bpp = 32,
        },
    },
    {
        .fourcc = DRM_FORMAT_P016,
        .format = VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,
        .subsampling = 420,
        .plane_count = 2,
        .planes[0] = {
            .aspect = VK_IMAGE_ASPECT_PLANE_0_BIT,
            .bpp = 16,
        },
        .planes[1] = {
            .aspect = VK_IMAGE_ASPECT_PLANE_1_BIT,
            .bpp = 32,
        },
    },
};

struct external_test {
    struct {
        bool buffer;
        bool protected;
        bool disjoint;
        bool compressed;

        bool cpu_direct;
        bool cpu_read;
        bool cpu_write;

        bool gpu_read;
        bool gpu_write;
        bool display_overlay;
        bool display_cursor;
        bool camera_read;
        bool camera_write;
        bool video_read;
        bool video_write;
        bool sensor_write;
    } use;

    uint32_t width;
    uint32_t height;
    const char *render_node;
    uint32_t offset_align;
    uint32_t pitch_align;

    struct vk_allocator alloc;

    VkMemoryPropertyFlags mt_flags;
    uint32_t mt_mask;

    VkBufferCreateFlags buf_flags;
    VkBufferUsageFlags buf_usage;

    VkImageCreateFlags img_flags;
    VkImageUsageFlags img_usage;
    VkImageCompressionFlagBitsEXT img_compression;
    bool img_linear_only;
};

static void
external_test_init_image_info(struct external_test *test)
{
    assert(!test->use.buffer && !test->use.sensor_write);

    if (test->use.protected)
        test->img_flags |= VK_IMAGE_CREATE_PROTECTED_BIT;

    if (test->use.disjoint)
        test->img_flags |= VK_IMAGE_CREATE_DISJOINT_BIT;

    if (!test->use.compressed)
        test->img_compression = VK_IMAGE_COMPRESSION_DISABLED_EXT;

    if (test->use.cpu_direct) {
        /* TODO VK_EXT_host_image_copy */
        test->img_linear_only = true;
    } else {
        if (test->use.cpu_read)
            test->img_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (test->use.cpu_write)
            test->img_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if (test->use.gpu_read)
        test->img_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (test->use.gpu_write)
        test->img_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    /* assume these require linear tiling */
    if (test->use.display_cursor || test->use.camera_read || test->use.camera_write ||
        test->use.video_read || test->use.video_write)
        test->img_linear_only = true;
}

static void
external_test_init_buffer_info(struct external_test *test)
{
    assert(test->use.buffer);

    if (test->use.protected)
        test->buf_flags |= VK_BUFFER_CREATE_PROTECTED_BIT;

    if (test->use.gpu_read)
        test->buf_usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (test->use.gpu_write)
        test->buf_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
}

static void
external_test_init_memory_info(struct external_test *test)
{
    struct vk_allocator *alloc = &test->alloc;

    if (test->use.protected)
        test->mt_flags |= VK_MEMORY_PROPERTY_PROTECTED_BIT;

    if (test->use.cpu_direct) {
        test->mt_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                          VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    }

    test->mt_mask = vk_allocator_query_memory_type_mask(alloc, test->mt_flags);
    if (!test->mt_mask && (test->mt_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        test->mt_flags &= ~VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        test->mt_mask = vk_allocator_query_memory_type_mask(alloc, test->mt_flags);
    }
    if (!test->mt_mask)
        vk_die("no valid memory type");
}

static void
external_test_init_use(struct external_test *test)
{
    test->use.protected = false;
    test->use.disjoint = false;
    test->use.compressed = true;

    test->use.cpu_direct = false;
    test->use.cpu_read = true;
    test->use.cpu_write = true;

    test->use.gpu_read = true;
    test->use.gpu_write = false;
    test->use.display_overlay = !test->use.buffer;
    test->use.display_cursor = false;
    test->use.camera_read = false;
    test->use.camera_write = false;
    test->use.video_read = false;
    test->use.video_write = false;
    test->use.sensor_write = false;

    const bool expect_image = test->use.display_overlay | test->use.display_cursor |
                              test->use.camera_read | test->use.camera_write |
                              test->use.video_read | test->use.video_write;
    if (test->use.buffer)
        assert(!expect_image);

    /* TODO VK_EXT_image_compression_control; we want to disable compression
     * for front-rendering
     */
    assert(test->use.compressed);

    if (test->use.cpu_read || test->use.cpu_write) {
        assert(!test->use.protected);
        /* require direct mapping */
        if (test->use.buffer)
            test->use.cpu_direct = true;
    } else {
        test->use.cpu_direct = false;
    }

    if (test->use.sensor_write)
        assert(test->use.buffer);
}

static void
external_test_init(struct external_test *test)
{
    struct vk_allocator *alloc = &test->alloc;

    external_test_init_use(test);
    vk_allocator_init(alloc, test->render_node, test->use.protected);

    external_test_init_memory_info(test);
    if (test->use.buffer)
        external_test_init_buffer_info(test);
    else
        external_test_init_image_info(test);
}

static void
external_test_cleanup(struct external_test *test)
{
    struct vk_allocator *alloc = &test->alloc;

    vk_allocator_cleanup(alloc);
}

static void
external_test_image(struct external_test *test,
                    const struct external_test_format *fmt,
                    uint64_t modifier,
                    uint32_t mem_plane_count)
{
    struct vk_allocator *alloc = &test->alloc;

    const struct vk_allocator_image_info info = {
        .flags = test->img_flags,
        .format = fmt->format,
        .modifier = modifier,
        .mem_plane_count = mem_plane_count,
        .usage = test->img_usage,
        .compression = test->img_compression,
        .mt_mask = test->mt_mask,
        .mt_coherent = test->mt_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };
    const bool supported = vk_allocator_query_image_support(alloc, &info);
    vk_log("fourcc '%c%c%c%c' modifier 0x%" PRIx64 ": %s", (fmt->fourcc >> 0) & 0xff,
           (fmt->fourcc >> 8) & 0xff, (fmt->fourcc >> 16) & 0xff, (fmt->fourcc >> 24) & 0xff,
           modifier, supported ? "supported" : "unsupported");
    if (!supported)
        return;

    struct vk_allocator_bo *bo = vk_allocator_bo_create_image(
        alloc, &info, test->width, test->height, test->offset_align, test->pitch_align, NULL);
    if (!bo) {
        if (test->offset_align > 1 || test->pitch_align > 1)
            vk_log("failed to create bo");
        else
            vk_die("failed to create bo");
        return;
    }

    /* write */
    if (test->use.cpu_write) {
        for (uint32_t i = 0; i < fmt->plane_count; i++) {
            const uint32_t width =
                (i > 0 && fmt->subsampling <= 422) ? test->width / 2 : test->width;
            const uint32_t height =
                (i > 0 && fmt->subsampling == 420) ? test->height / 2 : test->height;
            struct vk_allocator_transfer *xfer =
                vk_allocator_bo_map_transfer(alloc, bo, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                             fmt->planes[i].aspect, 0, 0, width, height);
            if (!xfer)
                vk_die("failed to map bo");

            const uint32_t dword_count = width * height * fmt->planes[i].bpp / 32;
            uint32_t *dwords = xfer->staging->mem_ptr;
            for (uint32_t j = 0; j < dword_count; j++)
                dwords[j] = j;

            vk_allocator_bo_unmap_transfer(alloc, bo, xfer);
        }
    }

    /* export */
    int fds[VK_ALLOCATOR_MEMORY_PLANE_MAX];
    for (uint32_t i = 0; i < VK_ALLOCATOR_MEMORY_PLANE_MAX; i++)
        fds[i] = -1;
    if (!vk_allocator_bo_export_fds(alloc, bo, fds))
        vk_die("failed to export bo");
    vk_allocator_bo_destroy(alloc, bo);

    /* import */
    bo = vk_allocator_bo_create_image(alloc, &info, test->width, test->height, test->offset_align,
                                      test->pitch_align, fds);
    for (uint32_t i = 0; i < VK_ALLOCATOR_MEMORY_PLANE_MAX; i++)
        close(fds[i]);
    if (!bo)
        vk_die("failed to create bo");

    /* read */
    if (test->use.cpu_read) {
        for (uint32_t i = 0; i < fmt->plane_count; i++) {
            const uint32_t width =
                (i > 0 && fmt->subsampling <= 422) ? test->width / 2 : test->width;
            const uint32_t height =
                (i > 0 && fmt->subsampling == 420) ? test->height / 2 : test->height;

            struct vk_allocator_transfer *xfer =
                vk_allocator_bo_map_transfer(alloc, bo, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                             fmt->planes[i].aspect, 0, 0, width, height);
            if (!xfer)
                vk_die("failed to map bo");

            const uint32_t dword_count = width * height * fmt->planes[i].bpp / 32;
            const uint32_t *dwords = xfer->staging->mem_ptr;
            for (uint32_t j = 0; j < dword_count; j++) {
                if (fmt->format == VK_FORMAT_R16G16B16A16_SFLOAT) {
                    const uint16_t half = dwords[j];
                    const int exponent = (half & 0x7c00) >> 10;
                    const int mantissa = half & 0x3ff;
                    const bool nan = exponent == 0x1f && mantissa;
                    if (nan)
                        continue;
                }

                assert(dwords[j] == j);
            }

            vk_allocator_bo_unmap_transfer(alloc, bo, xfer);
        }
    }

    vk_allocator_bo_destroy(alloc, bo);
}

static void
external_test_buffer(struct external_test *test)
{
    struct vk_allocator *alloc = &test->alloc;

    const struct vk_allocator_buffer_info buf_info = {
        .flags = test->buf_flags,
        .usage = test->buf_usage,
        .mt_mask = test->mt_mask,
        .mt_coherent = test->mt_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };
    if (!vk_allocator_query_buffer_support(alloc, &buf_info))
        return;

    const VkDeviceSize size = test->width * test->height;
    const uint32_t dword_count = size / sizeof(uint32_t);

    /* alloc */
    struct vk_allocator_bo *bo = vk_allocator_bo_create_buffer(alloc, &buf_info, size, -1);
    if (!bo)
        vk_die("failed to create bo");

    /* write */
    if (test->use.cpu_write) {
        assert(test->use.cpu_direct);
        uint32_t *dwords = vk_allocator_bo_map(alloc, bo, 0);
        if (!dwords)
            vk_die("failed to map bo");

        for (uint32_t i = 0; i < dword_count; i++)
            dwords[i] = i;

        vk_allocator_bo_unmap(alloc, bo, 0);
    }

    /* export */
    int fd;
    if (!vk_allocator_bo_export_fds(alloc, bo, &fd))
        vk_die("failed to export bo");
    vk_allocator_bo_destroy(alloc, bo);

    /* import */
    bo = vk_allocator_bo_create_buffer(alloc, &buf_info, size, fd);
    close(fd);
    if (!bo)
        vk_die("failed to create bo");

    /* read */
    if (test->use.cpu_read) {
        assert(test->use.cpu_direct);
        const uint32_t *dwords = vk_allocator_bo_map(alloc, bo, 0);
        if (!dwords)
            vk_die("failed to map bo");

        for (uint32_t i = 0; i < dword_count; i++)
            assert(dwords[i] == i);

        vk_allocator_bo_unmap(alloc, bo, 0);
    }

    vk_allocator_bo_destroy(alloc, bo);
}

static void
external_test_all(struct external_test *test)
{
    struct vk_allocator *alloc = &test->alloc;

    if (test->use.buffer) {
        external_test_buffer(test);
    } else {
        for (uint32_t i = 0; i < ARRAY_SIZE(external_test_formats); i++) {
            const struct external_test_format *fmt = &external_test_formats[i];
            /* we use exact matches in the table, while in most cases it is a
             * matter of channel swizzles and we don't really care as an
             * allocator (but we do as a mapper)
             */
            if (fmt->format == VK_FORMAT_UNDEFINED)
                continue;

            uint32_t mod_count;
            uint64_t *modifiers =
                vk_allocator_query_format_modifiers(alloc, fmt->format, &mod_count);
            uint32_t *mem_plane_counts = (void *)&modifiers[mod_count];

            for (uint32_t i = 0; i < mod_count; i++) {
                if (test->img_linear_only && modifiers[i] != DRM_FORMAT_MOD_LINEAR)
                    continue;
                external_test_image(test, fmt, modifiers[i], mem_plane_counts[i]);
            }

            free(modifiers);
        }
    }
}

static void
external_test_parse_args(struct external_test *test, int argc, char **argv)
{
    if (argc < 2) {
        vk_log("Usage: %s <buffer|image> [render-node] [offset-align] [pitch-align]", argv[0]);
        exit(-1);
    }
    argv++;

    test->use.buffer = !strcmp(*argv++, "buffer");
    if (argc >= 3)
        test->render_node = *argv++;
    if (argc >= 4)
        test->offset_align = atoi(*argv++);
    if (argc >= 5)
        test->pitch_align = atoi(*argv++);
}

int
main(int argc, char **argv)
{
    struct external_test test = {
        .width = 300,
        .height = 300,
        .offset_align = 1,
        .pitch_align = 1,
    };

    external_test_parse_args(&test, argc, argv);
    external_test_init(&test);
    external_test_all(&test);
    external_test_cleanup(&test);

    return 0;
}
