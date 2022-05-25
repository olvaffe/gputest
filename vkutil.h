/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef VKUTIL_H
#define VKUTIL_H

#include <dlfcn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <vulkan/vulkan.h>

#ifdef __ANDROID__
#define LIBVULKAN_NAME "libvulkan.so"
#else
#define LIBVULKAN_NAME "libvulkan.so.1"
#endif

#define PRINTFLIKE(f, a) __attribute__((format(printf, f, a)))
#define NORETURN __attribute__((noreturn))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define VKUTIL_MIN_API_VERSION VK_API_VERSION_1_1

struct vk {
    struct {
        void *handle;
#define PFN_ALL(name) PFN_vk##name name;
#include "vkutil_entrypoints.inc"
    };

    VkResult result;

    VkInstance instance;

    VkPhysicalDevice physical_dev;
    VkPhysicalDeviceFeatures2 features;
    VkPhysicalDeviceFeatures2 custom_border_color_features;
    VkPhysicalDeviceMemoryProperties mem_props;
    uint32_t buf_mt_index;

    bool EXT_custom_border_color;

    VkDevice dev;
    VkQueue queue;
    uint32_t queue_family_index;

    VkDescriptorPool desc_pool;

    VkCommandPool cmd_pool;
    VkCommandBuffer cmd;
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
    VkImage img;

    VkDeviceMemory mem;
    VkDeviceSize mem_size;
    bool mem_mappable;

    VkImageView render_view;

    VkImageView sample_view;
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
    VkShaderModule vs;
    VkShaderModule fs;

    VkDescriptorSetLayout set_layout;
    VkPipelineLayout pipeline_layout;

    VkVertexInputBindingDescription vi_binding;
    VkVertexInputAttributeDescription vi_attrs[16];
    uint32_t vi_attr_count;

    VkPipelineInputAssemblyStateCreateInfo ia_info;

    VkViewport viewport;
    VkRect2D scissor;

    VkPipelineRasterizationStateCreateInfo rast_info;

    VkPipelineMultisampleStateCreateInfo msaa_info;
    VkSampleMask sample_mask;

    VkPipelineDepthStencilStateCreateInfo depth_info;
    VkPipelineColorBlendAttachmentState color_att;

    const struct vk_framebuffer *fb;

    VkPipeline pipeline;
};

struct vk_descriptor_set {
    VkDescriptorSet set;
};

static inline void
vk_logv(const char *format, va_list ap)
{
    printf("VK: ");
    vprintf(format, ap);
    printf("\n");
}

static inline void NORETURN
vk_diev(const char *format, va_list ap)
{
    vk_logv(format, ap);
    abort();
}

static inline void PRINTFLIKE(1, 2) vk_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vk_logv(format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(1, 2) NORETURN vk_die(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vk_diev(format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(2, 3) vk_check(const struct vk *vk, const char *format, ...)
{
    if (vk->result == VK_SUCCESS)
        return;

    va_list ap;
    va_start(ap, format);
    if (vk->result > VK_SUCCESS)
        vk_logv(format, ap);
    else
        vk_diev(format, ap);
    va_end(ap);
}

static inline void
vk_init_global_dispatch(struct vk *vk)
{
#define PFN_GLOBAL(name)                                                                         \
    if (!(vk->name = (PFN_vk##name)vk->GetInstanceProcAddr(NULL, "vk" #name)))                   \
        vk_die("no global command vk" #name);
#include "vkutil_entrypoints.inc"
}

static inline void
vk_init_library(struct vk *vk)
{
    vk->handle = dlopen(LIBVULKAN_NAME, RTLD_LOCAL | RTLD_LAZY);
    if (!vk->handle)
        vk_die("failed to load %s: %s", LIBVULKAN_NAME, dlerror());

    const char gipa_name[] = "vkGetInstanceProcAddr";
    vk->GetInstanceProcAddr = dlsym(vk->handle, gipa_name);
    if (!vk->GetInstanceProcAddr)
        vk_die("failed to find %s: %s", gipa_name, dlerror());

    vk_init_global_dispatch(vk);
}

static inline void
vk_init_instance_dispatch(struct vk *vk)
{
#define PFN_INSTANCE(name)                                                                       \
    if (!(vk->name = (PFN_vk##name)vk->GetInstanceProcAddr(vk->instance, "vk" #name)))           \
        vk_die("no instance command vk" #name);
#include "vkutil_entrypoints.inc"
}

static inline void
vk_init_instance(struct vk *vk)
{
    uint32_t api_version;
    vk->EnumerateInstanceVersion(&api_version);
    if (api_version < VKUTIL_MIN_API_VERSION)
        vk_die("instance api version %d < %d", api_version, VKUTIL_MIN_API_VERSION);

    const VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo =
            &(VkApplicationInfo){
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .apiVersion = VKUTIL_MIN_API_VERSION,
            },
    };

    vk->result = vk->CreateInstance(&instance_info, NULL, &vk->instance);
    vk_check(vk, "failed to create instane");

    vk_init_instance_dispatch(vk);
}

static inline void
vk_init_physical_device(struct vk *vk)
{
    uint32_t count = 1;
    vk->result = vk->EnumeratePhysicalDevices(vk->instance, &count, &vk->physical_dev);
    if (vk->result < VK_SUCCESS || !count)
        vk_die("failed to enumerate physical devices");

    VkPhysicalDeviceProperties props;
    vk->GetPhysicalDeviceProperties(vk->physical_dev, &props);
    if (props.apiVersion < VKUTIL_MIN_API_VERSION)
        vk_die("physical device api version %d < %d", props.apiVersion, VKUTIL_MIN_API_VERSION);

    vk->custom_border_color_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT;
    vk->features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    vk->features.pNext = &vk->custom_border_color_features;
    vk->GetPhysicalDeviceFeatures2(vk->physical_dev, &vk->features);

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
vk_init_device_dispatch(struct vk *vk)
{
    vk->GetDeviceProcAddr =
        (PFN_vkGetDeviceProcAddr)vk->GetInstanceProcAddr(vk->instance, "vkGetDeviceProcAddr");

#define PFN_DEVICE(name)                                                                         \
    if (!(vk->name = (PFN_vk##name)vk->GetDeviceProcAddr(vk->dev, "vk" #name)))                  \
        vk_die("no device command vk" #name);
#include "vkutil_entrypoints.inc"
}

static inline void
vk_init_device(struct vk *vk)
{
    const char *exts[32];
    uint32_t ext_count = 0;
    const VkPhysicalDeviceFeatures2 *features = NULL;

#if 0
    exts[ext_count++] = "VK_EXT_custom_border_color";
    vk->EXT_custom_border_color = true;
    features = &vk->features;
#endif

    vk->queue_family_index = 0;

    VkQueueFamilyProperties queue_props;
    uint32_t queue_count = 1;
    vk->GetPhysicalDeviceQueueFamilyProperties(vk->physical_dev, &queue_count, &queue_props);
    if (!(queue_props.queueFlags & VK_QUEUE_GRAPHICS_BIT))
        vk_die("queue family 0 does not support graphics");

    const VkDeviceCreateInfo dev_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos =
            &(VkDeviceQueueCreateInfo){
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = vk->queue_family_index,
                .queueCount = 1,
                .pQueuePriorities = &(float){ 1.0f },
            },
        .enabledExtensionCount = ext_count,
        .ppEnabledExtensionNames = exts,
    };
    vk->result = vk->CreateDevice(vk->physical_dev, &dev_info, NULL, &vk->dev);
    vk_check(vk, "failed to create device");

    vk_init_device_dispatch(vk);

    vk->GetDeviceQueue(vk->dev, vk->queue_family_index, 0, &vk->queue);
}

static inline void
vk_init_desc_pool(struct vk *vk)
{
    const VkDescriptorPoolSize pool_sizes[] = {
        [0] = {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
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
    const VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = vk->queue_family_index,
    };

    vk->result = vk->CreateCommandPool(vk->dev, &pool_info, NULL, &vk->cmd_pool);
    vk_check(vk, "failed to create command pool");
}

static inline void
vk_init(struct vk *vk)
{
    memset(vk, 0, sizeof(*vk));

    vk_init_library(vk);

    vk_init_instance(vk);

    vk_init_physical_device(vk);
    vk_init_device(vk);

    vk_init_desc_pool(vk);
    vk_init_cmd_pool(vk);
}

static inline void
vk_cleanup(struct vk *vk)
{
    vk->DeviceWaitIdle(vk->dev);

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

static inline struct vk_buffer *
vk_create_buffer(struct vk *vk, VkDeviceSize size, VkBufferUsageFlags usage)
{
    struct vk_buffer *buf = calloc(1, sizeof(*buf));
    if (!buf)
        vk_die("failed to alloc buf");

    buf->info = (VkBufferCreateInfo){
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
    };

    vk->result = vk->CreateBuffer(vk->dev, &buf->info, NULL, &buf->buf);
    vk_check(vk, "failed to create buffer");

    VkMemoryRequirements reqs;
    vk->GetBufferMemoryRequirements(vk->dev, buf->buf, &reqs);
    if (!(reqs.memoryTypeBits & (1u << vk->buf_mt_index)))
        vk_die("failed to meet buf memory reqs: 0x%x", reqs.memoryTypeBits);

    buf->mem = vk_alloc_memory(vk, reqs.size, vk->buf_mt_index);
    buf->mem_size = reqs.size;

    vk->result = vk->MapMemory(vk->dev, buf->mem, 0, buf->mem_size, 0, &buf->mem_ptr);
    vk_check(vk, "failed to map buffer memory");

    vk->result = vk->BindBufferMemory(vk->dev, buf->buf, buf->mem, 0);
    vk_check(vk, "failed to bind buffer memory");

    return buf;
}

static inline void
vk_destroy_buffer(struct vk *vk, struct vk_buffer *buf)
{
    vk->FreeMemory(vk->dev, buf->mem, NULL);
    vk->DestroyBuffer(vk->dev, buf->buf, NULL);
    free(buf);
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
    struct vk_image *img = calloc(1, sizeof(*img));
    if (!img)
        vk_die("failed to alloc img");

    img->info = (VkImageCreateInfo){
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
vk_create_image_sample_view(struct vk *vk, struct vk_image *img, VkImageAspectFlagBits aspect)
{
    const VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = img->img,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = img->info.format,
        .subresourceRange = {
            .aspectMask = aspect,
            .levelCount = img->info.mipLevels,
            .layerCount = img->info.arrayLayers,
        },
    };
    vk->result = vk->CreateImageView(vk->dev, &view_info, NULL, &img->sample_view);
    vk_check(vk, "failed to create image sample view");

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

    const VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext =
            &(VkSamplerCustomBorderColorCreateInfoEXT){
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT,
                .customBorderColor = custom_border_color,
                .format = img->info.format,
            },
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
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

    vk->DestroyImageView(vk->dev, img->render_view, NULL);

    vk->FreeMemory(vk->dev, img->mem, NULL);
    vk->DestroyImage(vk->dev, img->img, NULL);
    free(img);
}

static inline void
vk_fill_image(struct vk *vk, struct vk_image *img, VkImageAspectFlagBits aspect, uint8_t val)
{
    if (!img->mem_mappable)
        vk_die("cannot fill non-mappable image");

    if (img->info.tiling != VK_IMAGE_TILING_LINEAR)
        vk_log("filling non-linear image");

    void *ptr;
    vk->result = vk->MapMemory(vk->dev, img->mem, 0, img->mem_size, 0, &ptr);
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
    switch (format) {
    case VK_FORMAT_B8G8R8A8_UNORM:
        cpp = 4;
        swizzle[0] = 2;
        swizzle[1] = 1;
        swizzle[2] = 0;
        break;
    default:
        vk_die("cannot write unknown format %d", format);
        break;
    }

    FILE *fp = fopen(filename, "w");
    if (!fp)
        vk_die("failed to open %s", filename);

    fprintf(fp, "P6 %u %u %u\n", width, height, ((cpp / 4) << 8) - 1);
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            const uint8_t *pixel = data + pitch * y + cpp * x;
            const char bytes[3] = { pixel[swizzle[0]], pixel[swizzle[1]], pixel[swizzle[2]] };
            if (fwrite(bytes, sizeof(bytes), 1, fp) != 1)
                vk_die("failed to write pixel (%u, %u)", x, y);
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

static inline struct vk_framebuffer *
vk_create_framebuffer(struct vk *vk,
                      struct vk_image *color,
                      struct vk_image *resolve,
                      struct vk_image *depth)
{
    struct vk_framebuffer *fb = calloc(1, sizeof(*fb));
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
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
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
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
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
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
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

    const VkRenderPassCreateInfo pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = att_count,
        .pAttachments = att_descs,
        .subpassCount = 1,
        .pSubpasses =
            &(VkSubpassDescription){
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = color ? 1 : 0,
                .pColorAttachments = &color_ref,
                .pResolveAttachments = &resolve_ref,
                .pDepthStencilAttachment = &depth_ref,
            },
    };

    vk->result = vk->CreateRenderPass(vk->dev, &pass_info, NULL, &fb->pass);
    vk_check(vk, "failed to create render pass");

    const VkFramebufferCreateInfo fb_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = fb->pass,
        .attachmentCount = att_count,
        .pAttachments = views,
        .width = color->info.extent.width,
        .height = color->info.extent.height,
        .layers = color->info.arrayLayers,
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
    struct vk_pipeline *pipeline = calloc(1, sizeof(*pipeline));
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
vk_set_pipeline_shaders(struct vk *vk,
                        struct vk_pipeline *pipeline,
                        const uint32_t *vs_code,
                        size_t vs_size,
                        const uint32_t *fs_code,
                        size_t fs_size)
{
    pipeline->vs = vk_create_shader_module(vk, vs_code, vs_size);
    pipeline->fs = vk_create_shader_module(vk, fs_code, fs_size);
}

static inline void
vk_set_pipeline_layout(struct vk *vk, struct vk_pipeline *pipeline, bool has_set)
{
    if (has_set) {
        const VkDescriptorSetLayoutCreateInfo set_layout_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings =
                &(VkDescriptorSetLayoutBinding){
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
        };

        vk->result =
            vk->CreateDescriptorSetLayout(vk->dev, &set_layout_info, NULL, &pipeline->set_layout);
        vk_check(vk, "failed to create descriptor set layout");
    }

    const VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = has_set,
        .pSetLayouts = &pipeline->set_layout,
    };
    vk->result = vk->CreatePipelineLayout(vk->dev, &pipeline_layout_info, NULL,
                                          &pipeline->pipeline_layout);
    vk_check(vk, "failed to create pipeline layout");
}

static inline void
vk_set_pipeline_vertices(struct vk *vk,
                         struct vk_pipeline *pipeline,
                         const uint32_t *comp_counts,
                         uint32_t attr_count)
{
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
vk_setup_pipeline(struct vk *vk, struct vk_pipeline *pipeline, const struct vk_framebuffer *fb)
{
    pipeline->ia_info = (VkPipelineInputAssemblyStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    };

    pipeline->viewport = (VkViewport){
        .width = (float)fb->width,
        .height = (float)fb->height,
        .maxDepth = 1.0f,
    };

    pipeline->scissor = (VkRect2D){
        .extent = {
            .width = fb->width,
            .height = fb->height,
        },
    };

    pipeline->rast_info = (VkPipelineRasterizationStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .lineWidth = 1.0f,
    };

    pipeline->sample_mask = (1u << fb->samples) - 1;
    pipeline->msaa_info = (VkPipelineMultisampleStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = fb->samples,
        .pSampleMask = &pipeline->sample_mask,
    };

    pipeline->depth_info = (VkPipelineDepthStencilStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    };

    pipeline->color_att = (VkPipelineColorBlendAttachmentState){
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    pipeline->fb = fb;
}

static inline void
vk_compile_pipeline(struct vk *vk, struct vk_pipeline *pipeline)
{
    const VkPipelineShaderStageCreateInfo stage_info[2] = {
        [0] = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = pipeline->vs,
            .pName = "main",
        },
        [1] = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = pipeline->fs,
            .pName = "main",
        },
    };

    const VkPipelineVertexInputStateCreateInfo vi_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
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
        .stageCount = 2,
        .pStages = stage_info,
        .pVertexInputState = &vi_info,
        .pInputAssemblyState = &pipeline->ia_info,
        .pViewportState = &vp_info,
        .pRasterizationState = &pipeline->rast_info,
        .pMultisampleState = &pipeline->msaa_info,
        .pDepthStencilState = &pipeline->depth_info,
        .pColorBlendState = &color_info,
        .layout = pipeline->pipeline_layout,
        .renderPass = pipeline->fb->pass,
    };

    vk->result = vk->CreateGraphicsPipelines(vk->dev, VK_NULL_HANDLE, 1, &pipeline_info, NULL,
                                             &pipeline->pipeline);
    vk_check(vk, "failed to create pipeline");
}

static inline void
vk_destroy_pipeline(struct vk *vk, struct vk_pipeline *pipeline)
{
    vk->DestroyShaderModule(vk->dev, pipeline->vs, NULL);
    vk->DestroyShaderModule(vk->dev, pipeline->fs, NULL);

    vk->DestroyDescriptorSetLayout(vk->dev, pipeline->set_layout, NULL);
    vk->DestroyPipelineLayout(vk->dev, pipeline->pipeline_layout, NULL);

    vk->DestroyPipeline(vk->dev, pipeline->pipeline, NULL);

    free(pipeline);
}

static inline struct vk_descriptor_set *
vk_create_descriptor_set(struct vk *vk, const struct vk_pipeline *pipeline)
{
    struct vk_descriptor_set *set = calloc(1, sizeof(*set));
    if (!set)
        vk_die("failed to alloc set");

    const VkDescriptorSetAllocateInfo set_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = vk->desc_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &pipeline->set_layout,
    };

    vk->result = vk->AllocateDescriptorSets(vk->dev, &set_info, &set->set);
    vk_check(vk, "failed to allocate descriptor set");

    return set;
}

static inline void
vk_write_descriptor_set(struct vk *vk, struct vk_descriptor_set *set, const struct vk_image *img)
{
    const VkWriteDescriptorSet write_info = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set->set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo =
            &(VkDescriptorImageInfo){
                .sampler = img->sampler,
                .imageView = img->sample_view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
    };

    vk->UpdateDescriptorSets(vk->dev, 1, &write_info, 0, NULL);
}

static inline void
vk_destroy_descriptor_set(struct vk *vk, struct vk_descriptor_set *set)
{
    free(set);
}

static inline VkCommandBuffer
vk_begin_cmd(struct vk *vk)
{
    const VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vk->cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    vk->result = vk->AllocateCommandBuffers(vk->dev, &alloc_info, &vk->cmd);
    vk_check(vk, "failed to allocate command buffer");

    const VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    vk->result = vk->BeginCommandBuffer(vk->cmd, &begin_info);
    vk_check(vk, "failed to begin command buffer");

    return vk->cmd;
}

static inline void
vk_end_cmd(struct vk *vk)
{
    vk->result = vk->EndCommandBuffer(vk->cmd);
    vk_check(vk, "failed to end command buffer");

    const VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &vk->cmd,
    };
    vk->result = vk->QueueSubmit(vk->queue, 1, &submit_info, VK_NULL_HANDLE);
    vk_check(vk, "failed to submit command buffer");

    vk->result = vk->QueueWaitIdle(vk->queue);
    vk_check(vk, "failed to wait submit");
}

#endif /* VKUTIL_H */
