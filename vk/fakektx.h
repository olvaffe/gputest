/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef FAKEKTX_H
#define FAKEKTX_H

#include "vkutil.h"

#define KTX_WRITER_KEY "KTXwriter"

typedef size_t ktx_size_t;
typedef void ktxHashListEntry;

typedef enum {
    KTX_SUCCESS = 0,
} KTX_error_code;

typedef enum {
    KTX_SS_NONE = 0,
} ktxSupercmpScheme;

enum {
    KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT = 0x1,
};

enum {
    ktxTexture1_c = 1,
    ktxTexture2_c = 2,
};

typedef struct {
    VkFormat format;
    uint32_t blockWidth;
    uint32_t blockHeight;
    uint32_t blockSize;
} ktxTexture_protected;

typedef struct {
    int classId;
    void *vtbl;
    void *vvtbl;
    ktxTexture_protected *_protected;
    bool isArray;
    bool isCubemap;
    bool isCompressed;
    bool generateMipmaps;
    uint32_t baseWidth;
    uint32_t baseHeight;
    uint32_t baseDepth;
    uint32_t numDimensions;
    uint32_t numLevels;
    uint32_t numLayers;
    uint32_t numFaces;
    struct {
        int x;
        int y;
        int z;
    } orientation;
    void *kvDataHead;
    uint32_t kvDataLen;
    uint8_t *kvData;
    size_t dataSize;
    uint8_t *pData;
} ktxTexture;

typedef struct {
    ktxTexture base;
    uint32_t glFormat;
    uint32_t glInternalformat;
    uint32_t glBaseInternalformat;
    uint32_t glType;
} ktxTexture1;

typedef struct {
    ktxTexture base;
    uint32_t vkFormat;
    uint32_t *pDfd;
    ktxSupercmpScheme supercompressionScheme;
    bool isVideo;
    uint32_t duration;
    uint32_t timescale;
    uint32_t loopcount;
} ktxTexture2;

static inline const char *
ktxErrorString(KTX_error_code error)
{
    return error == KTX_SUCCESS ? "KTX_SUCCESS" : "KTX_UNKNOWN";
}

static inline const char *
ktxSupercompressionSchemeString(ktxSupercmpScheme scheme)
{
    return scheme == KTX_SS_NONE ? "KTX_SS_NONE" : "KTX_SS_UNKNOWN";
}

static inline uint32_t
ktxTexture_GetRowPitch(ktxTexture *tex, uint32_t level)
{
    const uint32_t mip_width = u_minify(tex->baseWidth, level);
    const uint32_t block_count_x = DIV_ROUND_UP(mip_width, tex->_protected->blockWidth);
    return block_count_x * tex->_protected->blockSize;
}

static inline size_t
ktxTexture_GetImageSize(ktxTexture *tex, uint32_t level)
{
    const uint32_t mip_height = u_minify(tex->baseHeight, level);
    const uint32_t block_count_y = DIV_ROUND_UP(mip_height, tex->_protected->blockHeight);

    const uint32_t mip_depth = u_minify(tex->baseDepth, level);
    const uint32_t block_count_z = mip_depth;

    return ktxTexture_GetRowPitch(tex, level) * block_count_y * block_count_z * tex->numFaces *
           tex->numLayers;
}

static inline KTX_error_code
ktxTexture_GetImageOffset(
    ktxTexture *tex, uint32_t level, uint32_t layer, uint32_t faceSlice, size_t *pOffset)
{
    size_t offset = 0;
    for (uint32_t i = 0; i < level; i++)
        offset += ktxTexture_GetImageSize(tex, i);
    if (layer || faceSlice) {
        const size_t slice_size =
            ktxTexture_GetImageSize(tex, level) / (tex->numLayers * tex->numFaces);
        const uint32_t slice = tex->numFaces * layer + faceSlice;
        offset += slice_size * slice;
    }

    *pOffset = offset;
    return KTX_SUCCESS;
}

static inline KTX_error_code
ktxTexture_generate_data(ktxTexture *tex)
{
    ktxTexture_GetImageOffset(tex, tex->numLevels, 0, 0, &tex->dataSize);
    tex->pData = malloc(tex->dataSize);
    if (!tex->pData)
        return -1;

    void *block = tex->pData;
    for (uint32_t lv = 0; lv < tex->numLevels; lv++) {
        const uint32_t mip_width = u_minify(tex->baseWidth, lv);
        const uint32_t block_count_x = DIV_ROUND_UP(mip_width, tex->_protected->blockWidth);

        const uint32_t mip_height = u_minify(tex->baseHeight, lv);
        const uint32_t block_count_y = DIV_ROUND_UP(mip_height, tex->_protected->blockHeight);

        const uint32_t mip_depth = u_minify(tex->baseDepth, lv);
        const uint32_t block_count_z = mip_depth;

        for (uint32_t layer = 0; layer < tex->numLayers; layer++) {
            for (uint32_t face = 0; face < tex->numFaces; face++) {
                for (uint32_t bz = 0; bz < block_count_z; bz++) {
                    for (uint32_t by = 0; by < block_count_y; by++) {
                        for (uint32_t bx = 0; bx < block_count_x; bx++) {
                            const uint16_t red = (uint8_t)(bx * tex->_protected->blockWidth) << 8;
                            const uint16_t green = (uint8_t)(by * tex->_protected->blockHeight)
                                                   << 8;
                            const uint16_t blue = (uint8_t)((bz + face + layer) * 32) << 8;
                            const uint16_t alpha = 0xff;
                            const uint16_t block_data[8] = {
                                0xfdfc, 0xffff, 0xffff, 0xffff, red, green, blue, alpha,
                            };
                            assert(tex->_protected->blockSize == sizeof(block_data));
                            memcpy(block, block_data, sizeof(block_data));
                            block += sizeof(block_data);
                        }
                    }
                }
            }
        }
    }

    return KTX_SUCCESS;
}

static inline KTX_error_code
ktxTexture_CreateFromNamedFile(const char *const filename,
                               uint32_t createFlags,
                               ktxTexture **out_tex)
{
    vk_log("fakektx: 1 (ignoring %s)", filename);
    const VkFormat tex_format = VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
    const uint32_t tex_block_width = 4;
    const uint32_t tex_block_height = 4;

    static ktxTexture_protected tex_protected = {
        .format = tex_format,
        .blockWidth = tex_block_width,
        .blockHeight = tex_block_height,
        .blockSize = 16,
    };
    static ktxTexture2 tex = {
        .base = {
            .classId = ktxTexture2_c,
            ._protected = &tex_protected,
            .isArray = false,
            .isCubemap = false,
            .isCompressed = true,
            .generateMipmaps = false,
            .baseWidth = 256,
            .baseHeight = 256,
            .baseDepth = 1,
            .numDimensions = 2,
            .numLevels = 1,
            .numLayers = 1,
            .numFaces = 1,
            .orientation = {
                .x = 'r',
                .y = 'd',
                .z = 'o',
            },
            .kvDataHead = NULL,
            .kvDataLen = 0,
            .kvData = NULL,
            .dataSize = 0,
            .pData = NULL,
        },
        .vkFormat = tex_format,
        .pDfd = NULL,
        .supercompressionScheme = KTX_SS_NONE,
        .isVideo = false,
        .duration = 0,
        .timescale = 0,
        .loopcount = 0,
    };

    KTX_error_code err = ktxTexture_generate_data(&tex.base);
    if (err != KTX_SUCCESS)
        return err;

    *out_tex = &tex.base;

    return KTX_SUCCESS;
}

static inline void
ktxTexture_Destroy(ktxTexture *tex)
{
    free(tex->pData);
}

static inline size_t
ktxTexture_GetDataSizeUncompressed(ktxTexture *tex)
{
    if (tex->classId == ktxTexture2_c)
        assert(((ktxTexture2 *)tex)->supercompressionScheme == KTX_SS_NONE);
    return tex->dataSize;
}

static inline uint32_t
ktxTexture_GetElementSize(ktxTexture *tex)
{
    return tex->_protected->blockSize;
}

static inline ktxHashListEntry *
ktxHashList_Next(ktxHashListEntry *entry)
{
    return NULL;
}

static inline KTX_error_code
ktxHashListEntry_GetKey(ktxHashListEntry *entry, unsigned int *pKeyLen, char **ppKey)
{
    return -1;
}

static inline KTX_error_code
ktxHashListEntry_GetValue(ktxHashListEntry *entry, unsigned int *pValueLen, void **ppValue)
{
    return -1;
}

static inline VkFormat
ktxTexture_GetVkFormat(ktxTexture *tex)
{
    return tex->_protected->format;
}

static inline bool
ktxTexture_NeedsTranscoding(ktxTexture *tex)
{
    if (tex->classId == ktxTexture2_c)
        assert(!((ktxTexture2 *)tex)->pDfd);
    return false;
}

#endif /* FAKEKTX_H */
