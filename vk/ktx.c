/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

#ifdef FAKEKTX
#include "fakektx.h"
#else
#include <ktx.h>
#include <ktxvulkan.h>
#endif

static const uint32_t ktx_test_vs[] = {
#include "ktx_test.vert.inc"
};

static const uint32_t ktx_test_fs[] = {
#include "ktx_test.frag.inc"
};

struct ktx_test_push_const {
    uint32_t view_type;
    float slice;
};

struct ktx_test {
    VkFormat rt_format;
    const char *filename;
    int slice;
    ktxTexture *tex;

    struct vk vk;
    struct vk_buffer *staging_buf;
    struct vk_image *tex_img;

    struct vk_image *rt_img;
    struct vk_pipeline *pipeline;
    struct vk_descriptor_set *set;
    VkBufferImageCopy *copies;
};

static void
ktx_test_dump_info(struct ktx_test *test, ktxTexture *tex)
{
    vk_log("filename: %s:", test->filename);

    vk_log("ktxTexture:");
    vk_log("  classId: %d", tex->classId);
    vk_log("  isArray: %d", tex->isArray);
    vk_log("  isCubemap: %d", tex->isCubemap);
    vk_log("  isCompressed: %d", tex->isCompressed);
    vk_log("  generateMipmaps: %d", tex->generateMipmaps);
    vk_log("  baseWidth: %d", tex->baseWidth);
    vk_log("  baseHeight: %d", tex->baseHeight);
    vk_log("  baseDepth: %d", tex->baseDepth);
    vk_log("  numDimensions: %d", tex->numDimensions);
    vk_log("  numLevels: %d", tex->numLevels);
    vk_log("  numLayers: %d", tex->numLayers);
    vk_log("  numFaces: %d", tex->numFaces);
    vk_log("  orientation %c%c%c", tex->orientation.x, tex->orientation.y, tex->orientation.z);
    vk_log("  kvDataHead: %p", tex->kvDataHead);
    vk_log("  kvDataLen: %d", tex->kvDataLen);
    vk_log("  kvData: %p", tex->kvData);
    vk_log("  dataSize: %zu", tex->dataSize);
    vk_log("  pData: %p", tex->pData);

    if (tex->classId == ktxTexture1_c) {
        ktxTexture1 *tex1 = (ktxTexture1 *)tex;
        vk_log("ktxTexture1:");
        vk_log("  glFormat: 0x%04x", tex1->glFormat);
        vk_log("  glInternalformat: 0x%04x", tex1->glInternalformat);
        vk_log("  glBaseInternalformat: 0x%04x", tex1->glBaseInternalformat);
        vk_log("  glType: 0x%04x", tex1->glType);
    } else if (tex->classId == ktxTexture2_c) {
        ktxTexture2 *tex2 = (ktxTexture2 *)tex;
        vk_log("ktxTexture2:");
        vk_log("  vkFormat: %d", tex2->vkFormat);
        vk_log("  pDfd: %p", tex2->pDfd);
        vk_log("  supercompressionScheme: %s",
               ktxSupercompressionSchemeString(tex2->supercompressionScheme));
        vk_log("  isVideo: %d", tex2->isVideo);
        vk_log("  duration: %d", tex2->duration);
        vk_log("  timescale: %d", tex2->timescale);
        vk_log("  loopcount: %d", tex2->loopcount);
    }

    vk_log("derived:");
    vk_log("  GetDataSizeUncompressed: %zu", ktxTexture_GetDataSizeUncompressed(tex));
    vk_log("  NeedsTranscoding: %d", ktxTexture_NeedsTranscoding(tex));
    vk_log("  GetElementSize: %d", ktxTexture_GetElementSize(tex));

    for (uint32_t level = 0; level < tex->numLevels; level++) {
        vk_log("  mip level %d:", level);

        for (uint32_t layer = 0; layer < tex->numLayers; layer++) {
            for (uint32_t face = 0; face < tex->numFaces; face++) {
                ktx_size_t offset;
                ktxTexture_GetImageOffset(tex, level, layer, face, &offset);
                vk_log("    GetImageOffset layer %d face %d: %zu", layer, face, offset);
            }
        }
        vk_log("    GetImageSize: %zu", ktxTexture_GetImageSize(tex, level));
        vk_log("    GetRowPitch: %d", ktxTexture_GetRowPitch(tex, level));
    }

    vk_log("metadata:");
    for (ktxHashListEntry *entry = tex->kvDataHead; entry; entry = ktxHashList_Next(entry)) {
        char *key;
        unsigned int key_size;
        void *val;
        unsigned int val_size;

        ktxHashListEntry_GetKey(entry, &key_size, &key);
        ktxHashListEntry_GetValue(entry, &val_size, &val);
        if (!strcmp(key, KTX_WRITER_KEY))
            vk_log("  %s: %s", key, (const char *)val);
        else
            vk_log("  %s size: %d", key, val_size);
    }
}

static void
ktx_test_load_file(struct ktx_test *test)
{
    ktxTexture *tex;
    KTX_error_code result = ktxTexture_CreateFromNamedFile(
        test->filename, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &tex);
    if (result != KTX_SUCCESS)
        vk_die("failed to load %s: %s", test->filename, ktxErrorString(result));

    ktx_test_dump_info(test, tex);

    /* only KTX 2.0 guarantees tight packing */
    if (tex->classId != ktxTexture2_c)
        vk_die("only KTX 2.0 is supported");
    if (((ktxTexture2 *)tex)->supercompressionScheme != KTX_SS_NONE)
        vk_die("data is super-compressed");

    test->tex = tex;
}

static void
ktx_test_init_descriptor_set(struct ktx_test *test)
{
    struct vk *vk = &test->vk;

    test->set = vk_create_descriptor_set(vk, test->pipeline->set_layouts[0]);
    vk_write_descriptor_set_image(vk, test->set, test->tex_img);
}

static void
ktx_test_init_pipeline(struct ktx_test *test)
{
    struct vk *vk = &test->vk;

    test->pipeline = vk_create_pipeline(vk);

    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_VERTEX_BIT, ktx_test_vs,
                           sizeof(ktx_test_vs));
    vk_add_pipeline_shader(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, ktx_test_fs,
                           sizeof(ktx_test_fs));

    vk_add_pipeline_set_layout(vk, test->pipeline, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                               VK_SHADER_STAGE_FRAGMENT_BIT, NULL);
    vk_set_pipeline_push_const(vk, test->pipeline, VK_SHADER_STAGE_FRAGMENT_BIT,
                               sizeof(struct ktx_test_push_const));

    vk_set_pipeline_topology(vk, test->pipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

    vk_set_pipeline_viewport(vk, test->pipeline, test->rt_img->info.extent.width,
                             test->rt_img->info.extent.height);
    vk_set_pipeline_rasterization(vk, test->pipeline, VK_POLYGON_MODE_FILL, false);

    vk_set_pipeline_sample_count(vk, test->pipeline, test->rt_img->info.samples);

    vk_setup_pipeline(vk, test->pipeline, NULL);
    test->pipeline->rendering_info = (VkPipelineRenderingCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &test->rt_format,
    };

    vk_compile_pipeline(vk, test->pipeline);
}

static void
ktx_test_init_rt_image(struct ktx_test *test)
{
    struct vk *vk = &test->vk;
    ktxTexture *tex = test->tex;

    test->rt_img = vk_create_image(vk, test->rt_format, tex->baseWidth, tex->baseHeight,
                                   VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_LINEAR,
                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    vk_create_image_render_view(vk, test->rt_img, VK_IMAGE_ASPECT_COLOR_BIT);
}

static void
ktx_test_init_texture_image(struct ktx_test *test)
{
    struct vk *vk = &test->vk;
    ktxTexture *tex = test->tex;

    VkImageType img_type;
    VkImageViewType view_type;
    switch (tex->numDimensions) {
    case 1:
        assert(!tex->isCubemap);
        img_type = VK_IMAGE_TYPE_1D;
        view_type = tex->isArray ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
        break;
    case 2:
        img_type = VK_IMAGE_TYPE_2D;
        if (tex->isCubemap)
            view_type = tex->isArray ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
        else
            view_type = tex->isArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        break;
    case 3:
        assert(!tex->isCubemap && !tex->isArray);
        img_type = VK_IMAGE_TYPE_3D;
        view_type = VK_IMAGE_VIEW_TYPE_3D;
        break;
    default:
        vk_die("bad dim");
    }

    VkImageCreateFlags flags = 0;
    if (tex->isCubemap)
        flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
#if 0
    if (tex->isCompressed) {
        flags |=
            VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT;
    }
#endif

    const VkImageCreateInfo img_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = flags,
        .imageType = img_type,
        .format = ktxTexture_GetVkFormat(tex),
        .extent = {
            .width = tex->baseWidth,
            .height = tex->baseHeight,
            .depth = tex->baseDepth,
        },
        .mipLevels = tex->numLevels,
        .arrayLayers = tex->numLayers * tex->numFaces,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    test->tex_img = vk_create_image_from_info(vk, &img_info);
    vk_create_image_sample_view(vk, test->tex_img, view_type, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_create_image_sampler(vk, test->tex_img, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST);
}

static void
ktx_test_init_staging_buffer(struct ktx_test *test)
{
    struct vk *vk = &test->vk;
    ktxTexture *tex = test->tex;

    test->staging_buf = vk_create_buffer(vk, 0, tex->dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    memcpy(test->staging_buf->mem_ptr, tex->pData, tex->dataSize);
}

static void
ktx_test_init(struct ktx_test *test)
{
    struct vk *vk = &test->vk;

    ktx_test_load_file(test);

    const struct vk_init_params params = {
        .api_version = VK_API_VERSION_1_3,
        .enable_all_features = true,
    };
    vk_init(vk, &params);

    ktx_test_init_staging_buffer(test);
    ktx_test_init_texture_image(test);
    ktx_test_init_rt_image(test);
    ktx_test_init_pipeline(test);
    ktx_test_init_descriptor_set(test);
}

static void
ktx_test_cleanup(struct ktx_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_descriptor_set(vk, test->set);
    vk_destroy_pipeline(vk, test->pipeline);
    vk_destroy_image(vk, test->rt_img);

    vk_destroy_image(vk, test->tex_img);
    vk_destroy_buffer(vk, test->staging_buf);
    vk_cleanup(vk);

    ktxTexture_Destroy(test->tex);
}

static void
ktx_test_draw_quad(struct ktx_test *test, VkCommandBuffer cmd)
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
        .image = test->rt_img->img,
        .subresourceRange = subres_range,
    };
    const VkImageMemoryBarrier barrier2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = test->rt_img->img,
        .subresourceRange = subres_range,
    };

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1,
                           &barrier1);

    const VkRenderingAttachmentInfo att_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = test->rt_img->render_view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };
    const VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .extent = {
                .width = test->rt_img->info.extent.width,
                .height = test->rt_img->info.extent.height,
            },
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &att_info,
    };
    vk->CmdBeginRendering(cmd, &rendering_info);
    vk->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, test->pipeline->pipeline);

    const struct ktx_test_push_const push = {
        .view_type = test->tex_img->sample_view_type,
        .slice =
            (float)test->slice / (test->tex->baseDepth == 1 ? 1 : (test->tex->baseDepth - 1)),
    };
    vk->CmdPushConstants(cmd, test->pipeline->pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(push), &push);
    vk->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              test->pipeline->pipeline_layout, 0, 1, &test->set->set, 0, NULL);
    vk->CmdDraw(cmd, 4, 1, 0, 0);
    vk->CmdEndRendering(cmd);

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 0, NULL, 1, &barrier2);
}

static void
ktx_test_draw_prep_texture(struct ktx_test *test, VkCommandBuffer cmd)
{
    struct vk *vk = &test->vk;
    ktxTexture *tex = test->tex;

    const VkImageSubresourceRange subres_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = tex->numLevels,
        .layerCount = tex->numLayers * tex->numFaces,
    };
    const VkImageMemoryBarrier barrier1 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = test->tex_img->img,
        .subresourceRange = subres_range,
    };
    const VkImageMemoryBarrier barrier2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image = test->tex_img->img,
        .subresourceRange = subres_range,
    };

    VkBufferImageCopy *copies = malloc(sizeof(*copies) * tex->numLevels);
    if (!copies)
        vk_die("failed to alloc copies");
    for (uint32_t i = 0; i < tex->numLevels; i++) {
        ktx_size_t offset;
        ktxTexture_GetImageOffset(tex, i, 0, 0, &offset);

        copies[i] = (VkBufferImageCopy){
            .bufferOffset = offset,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = i,
                .layerCount = tex->numLayers * tex->numFaces,
            },
            .imageExtent = {
                .width = u_minify(tex->baseWidth, i),
                .height = u_minify(tex->baseHeight, i),
                .depth = u_minify(tex->baseDepth, i),
	    },
        };
    }

    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0, 0, NULL, 0, NULL, 1, &barrier1);
    vk->CmdCopyBufferToImage(cmd, test->staging_buf->buf, test->tex_img->img, barrier1.newLayout,
                             tex->numLevels, copies);
    vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1,
                           &barrier2);

    free(copies);
}

static void
ktx_test_draw(struct ktx_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);
    ktx_test_draw_prep_texture(test, cmd);
    ktx_test_draw_quad(test, cmd);
    vk_end_cmd(vk);
    vk_wait(vk);

    vk_dump_image(vk, test->rt_img, VK_IMAGE_ASPECT_COLOR_BIT, "rt.ppm");
}

int
main(int argc, char **argv)
{
    struct ktx_test test = {
        .rt_format = VK_FORMAT_B8G8R8A8_UNORM,
    };

    if (argc < 2) {
        vk_log("Usage: %s <filename.ktx> [slice]", argv[0]);
        return -1;
    }

    test.filename = argv[1];
    test.slice = argc > 2 ? atoi(argv[2]) : 0;

    ktx_test_init(&test);
    ktx_test_draw(&test);
    ktx_test_cleanup(&test);

    return 0;
}
