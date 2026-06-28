/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "androidutil.h"
#include "vkutil.h"

static const uint32_t ahb_rt_test_vs[] = {
#include "ahb_rt_test.vert.inc"
};

static const uint32_t ahb_rt_test_fs[] = {
#include "ahb_rt_test.frag.inc"
};

struct ahb_rt_test {
    enum AHardwareBuffer_Format ahb_format;
    uint32_t width;
    uint32_t height;

    struct vk vk;
    struct android android;

    struct android_ahb *ahb;
    VkAndroidHardwareBufferPropertiesANDROID ahb_props;
    VkAndroidHardwareBufferFormatPropertiesANDROID ahb_fmt_props;
    VkAndroidHardwareBufferFormatResolvePropertiesANDROID ahb_resolve_props;

    VkImage img;
    VkDeviceMemory mem;
    VkSamplerYcbcrConversion conv;
    VkImageView rt_view;
    VkImageView resolve_view;

    struct vk_pipeline *pipeline;
};

static void
ahb_rt_test_init_pipeline(struct ahb_rt_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, ahb_rt_test_vs,
                           sizeof(ahb_rt_test_vs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, ahb_rt_test_fs,
                           sizeof(ahb_rt_test_fs));

    vk_set_pipeline_topology(vk, test->pipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

    vk_set_pipeline_viewport(vk, test->pipeline, test->width, test->height);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_FILL, false);

    vk_set_pipeline_sample_count(vk, test->pipeline, VK_SAMPLE_COUNT_1_BIT);

    vk_setup_pipeline(vk, test->pipeline, NULL);
    test->pipeline->rendering_info = (VkPipelineRenderingCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &test->ahb_fmt_props.format,
    };
    if (test->ahb_fmt_props.format == VK_FORMAT_UNDEFINED) {
        test->pipeline->rendering_info.pColorAttachmentFormats =
            &test->ahb_resolve_props.colorAttachmentFormat;

        test->pipeline->external_format = (VkExternalFormatANDROID){
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID,
            .externalFormat = test->ahb_fmt_props.externalFormat,
        };
    }

    vk_compile_pipeline(vk, test->pipeline);
}

static void
ahb_rt_test_init_image_view(struct ahb_rt_test *test)
{
    struct vk *vk = &test->vk;

    const VkSamplerYcbcrConversionInfo ycbcr_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
        .conversion = test->conv,
    };
    const VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = test->conv ? &ycbcr_info : NULL,
        .image = test->img,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = test->ahb_fmt_props.format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    VkImageView view;
    vk->result = vk->CreateImageView(vk->dev, &view_info, NULL, &view);
    vk_check(vk, "failed to create image render view");

    if (test->ahb_fmt_props.format != VK_FORMAT_UNDEFINED) {
        test->rt_view = view;
    } else if (vk->external_format_resolve_props.nullColorAttachmentWithExternalFormatResolve)
        test->resolve_view = view;
    else
        vk_die("only nullColorAttachmentWithExternalFormatResolve support");
}

static void
ahb_rt_test_init_ycbcr_conv(struct ahb_rt_test *test)
{
    struct vk *vk = &test->vk;

    if (test->ahb_fmt_props.format != VK_FORMAT_UNDEFINED)
        return;

    const VkExternalFormatANDROID external_fmt = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID,
        .externalFormat = test->ahb_fmt_props.externalFormat,
    };
    const VkSamplerYcbcrConversionCreateInfo conv_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
        .pNext = &external_fmt,
        .format = test->ahb_fmt_props.format,
        .ycbcrModel = test->ahb_fmt_props.suggestedYcbcrModel,
        .ycbcrRange = test->ahb_fmt_props.suggestedYcbcrRange,
        .xChromaOffset = test->ahb_fmt_props.suggestedXChromaOffset,
        .yChromaOffset = test->ahb_fmt_props.suggestedYChromaOffset,
        .chromaFilter = VK_FILTER_NEAREST,
    };

    vk->result = vk->CreateSamplerYcbcrConversion(vk->dev, &conv_info, NULL, &test->conv);
    vk_check(vk, "failed to create VkSamplerYcbcrConversion");
}

static void
ahb_rt_test_init_memory(struct ahb_rt_test *test)
{
    struct vk *vk = &test->vk;

    const uint32_t mt = ffs(test->ahb_props.memoryTypeBits) - 1;

    const VkImportAndroidHardwareBufferInfoANDROID import_info = {
        .sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
        .buffer = test->ahb->ahb,
    };
    const VkMemoryDedicatedAllocateInfo dedicated_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        .pNext = &import_info,
        .image = test->img,
    };
    const VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &dedicated_info,
        .allocationSize = test->ahb_props.allocationSize,
        .memoryTypeIndex = mt,
    };

    vk->result = vk->AllocateMemory(vk->dev, &alloc_info, NULL, &test->mem);
    vk_check(vk, "failed to import ahb");

    vk->result = vk->BindImageMemory(vk->dev, test->img, test->mem, 0);
    vk_check(vk, "failed to bind image memory");
}

static void
ahb_rt_test_init_image(struct ahb_rt_test *test)
{
    struct vk *vk = &test->vk;
    bool ahb_ext_fmt = test->ahb_fmt_props.format == VK_FORMAT_UNDEFINED;

    /* the validity is implied */
    if (!ahb_ext_fmt) {
        const VkPhysicalDeviceExternalImageFormatInfo fmt_ext_info = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
            .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
        };
        const VkPhysicalDeviceImageFormatInfo2 fmt_info = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
            .pNext = &fmt_ext_info,
            .format = test->ahb_fmt_props.format,
            .type = VK_IMAGE_TYPE_2D,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        };
        VkExternalImageFormatProperties fmt_ext_props = {
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
        };
        VkImageFormatProperties2 fmt_props = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
            .pNext = &fmt_ext_props,
        };
        vk->result =
            vk->GetPhysicalDeviceImageFormatProperties2(vk->physical_dev, &fmt_info, &fmt_props);
        vk_check(vk, "unsupported image");

        const VkExternalMemoryFeatureFlags ext_mem_feats =
            fmt_ext_props.externalMemoryProperties.externalMemoryFeatures;
        if (!(ext_mem_feats & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT))
            vk_die("image does not support import");
    }

    const VkExternalFormatANDROID external_fmt = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID,
        .externalFormat = test->ahb_fmt_props.externalFormat,
    };
    const VkExternalMemoryImageCreateInfo external_info = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .pNext = ahb_ext_fmt ? &external_fmt : NULL,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
    };
    const VkImageCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &external_info,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = test->ahb_fmt_props.format,
        .extent = {
            .width = test->width,
            .height = test->height,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    vk->result = vk->CreateImage(vk->dev, &info, NULL, &test->img);
    vk_check(vk, "failed to create image");
}

static void
ahb_rt_test_init_ahb(struct ahb_rt_test *test)
{
    struct vk *vk = &test->vk;
    struct android *android = &test->android;

    test->ahb = android_create_ahb(
        android, test->width, test->height, test->ahb_format,
        AHARDWAREBUFFER_USAGE_CPU_READ_RARELY | AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT);

    test->ahb_resolve_props = (VkAndroidHardwareBufferFormatResolvePropertiesANDROID){
        .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_RESOLVE_PROPERTIES_ANDROID,
    };
    test->ahb_fmt_props = (VkAndroidHardwareBufferFormatPropertiesANDROID){
        .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID,
        .pNext = &test->ahb_resolve_props,
    };
    test->ahb_props = (VkAndroidHardwareBufferPropertiesANDROID){
        .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,
        .pNext = &test->ahb_fmt_props,
    };

    vk->GetAndroidHardwareBufferPropertiesANDROID(vk->dev, test->ahb->ahb, &test->ahb_props);

    if (test->ahb_resolve_props.colorAttachmentFormat == VK_FORMAT_UNDEFINED)
        vk_die("unsupported ahb format");
}

static void
ahb_rt_test_init(struct ahb_rt_test *test)
{
    struct vk *vk = &test->vk;
    struct android *android = &test->android;

    const char *dev_exts[] = {
        VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
        VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME,
        VK_ANDROID_EXTERNAL_FORMAT_RESOLVE_EXTENSION_NAME,
    };
    const struct vk_init_params params = {
        .api_version = VK_API_VERSION_1_3,
        .enable_all_features = true,
        .dev_exts = dev_exts,
        .dev_ext_count = ARRAY_SIZE(dev_exts),
    };

    vk_init(vk, &params);
    android_init(android, NULL);

    ahb_rt_test_init_ahb(test);
    ahb_rt_test_init_image(test);
    ahb_rt_test_init_memory(test);
    ahb_rt_test_init_ycbcr_conv(test);
    ahb_rt_test_init_image_view(test);
    ahb_rt_test_init_pipeline(test);
}

static void
ahb_rt_test_cleanup(struct ahb_rt_test *test)
{
    struct vk *vk = &test->vk;
    struct android *android = &test->android;

    vk_destroy_pipeline(vk, test->pipeline);

    vk->DestroyImageView(vk->dev, test->rt_view, NULL);
    vk->DestroyImageView(vk->dev, test->resolve_view, NULL);
    vk->DestroySamplerYcbcrConversion(vk->dev, test->conv, NULL);
    vk->FreeMemory(vk->dev, test->mem, NULL);
    vk->DestroyImage(vk->dev, test->img, NULL);

    android_destroy_ahb(android, test->ahb);

    android_cleanup(android);
    vk_cleanup(vk);
}

static void
ahb_rt_test_draw_triangle(struct ahb_rt_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    const VkImageMemoryBarrier barrier1 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = test->img,
        .subresourceRange = subres_range,
    };
    const VkImageMemoryBarrier barrier2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = vk->queue_family_index,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
        .image = test->img,
        .subresourceRange = subres_range,
    };

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1,
                           &barrier1);

    const VkRenderingAttachmentInfo att_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = test->rt_view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode = test->resolve_view ? VK_RESOLVE_MODE_EXTERNAL_FORMAT_DOWNSAMPLE_BIT_ANDROID
                                          : VK_RESOLVE_MODE_NONE,
        .resolveImageView = test->resolve_view,
        .resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {
            .color = {
                .float32 = { 0.2f, 0.2f, 0.2f, 1.0f },
            },
        },
    };
    const VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .extent = {
                .width = test->width,
                .height = test->height,
            },
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &att_info,
    };
    vk->CmdBeginRendering(cmd, &rendering_info);
    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, test->pipeline->pipeline);
    vk->CmdDraw(cmd, 3, 1, 0, 0);
    vk->CmdEndRendering(cmd);

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_NONE, 0, 0, NULL, 0, NULL, 1, &barrier2);
}

static void
ahb_rt_test_draw(struct ahb_rt_test *test)
{
    struct vk *vk = &test->vk;
    struct android *android = &test->android;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    ahb_rt_test_draw_triangle(test, cmd);

    vk_end_cmd(vk);
    vk_wait(vk);

    android_dump_ahb(android, test->ahb, "rt.ppm");
}

int
main(void)
{
    struct ahb_rt_test test = {
        .ahb_format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
        //.ahb_format = AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420,
        .width = 300,
        .height = 300,
    };

    ahb_rt_test_init(&test);
    ahb_rt_test_draw(&test);
    ahb_rt_test_cleanup(&test);

    return 0;
}
