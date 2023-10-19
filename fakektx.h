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

static inline KTX_error_code
ktxTexture_CreateFromNamedFile(const char *const filename,
                               uint32_t createFlags,
                               ktxTexture **out_tex)
{
    vk_log("fakektx: 1 (ignoring %s)", filename);
    const VkFormat tex_format = VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
    const uint32_t tex_block_width = 4;
    const uint32_t tex_block_height = 4;
    const uint32_t tex_block_size = 16;
    static uint8_t tex_data[16] = {
        0xfc, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x00, 0x00, 0x00, 0x40, 0x00, 0x80, 0x00, 0xc0,
    };

    static ktxTexture_protected tex_protected = {
        .format = tex_format,
        .blockWidth = tex_block_width,
        .blockHeight = tex_block_height,
        .blockSize = tex_block_size,
    };
    static ktxTexture2 tex = {
        .base = {
            .classId = ktxTexture2_c,
            ._protected = &tex_protected,
            .isArray = false,
            .isCubemap = false,
            .isCompressed = true,
            .generateMipmaps = false,
            .baseWidth = tex_block_width,
            .baseHeight = tex_block_height,
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
            .dataSize = sizeof(tex_data),
            .pData = tex_data,
        },
        .vkFormat = tex_format,
        .pDfd = NULL,
        .supercompressionScheme = KTX_SS_NONE,
        .isVideo = false,
        .duration = 0,
        .timescale = 0,
        .loopcount = 0,
    };

    *out_tex = &tex.base;

    return KTX_SUCCESS;
}

static inline void
ktxTexture_Destroy(ktxTexture *tex)
{
}

static inline size_t
ktxTexture_GetDataSizeUncompressed(ktxTexture *tex)
{
    if (tex->classId == ktxTexture2_c)
        assert(((ktxTexture2 *)tex)->supercompressionScheme == KTX_SS_NONE);
    return tex->dataSize;
}

static inline KTX_error_code
ktxTexture_GetImageOffset(
    ktxTexture *tex, uint32_t level, uint32_t layer, uint32_t faceSlice, size_t *pOffset)
{
    assert(tex->numLevels == 1 && tex->numLayers == 1 && tex->numFaces == 1);
    *pOffset = 0;
    return KTX_SUCCESS;
}

static inline size_t
ktxTexture_GetImageSize(ktxTexture *tex, uint32_t level)
{
    assert(tex->numLevels == 1);
    return ktxTexture_GetDataSizeUncompressed(tex);
}

static inline uint32_t
ktxTexture_GetRowPitch(ktxTexture *tex, uint32_t level)
{
    assert(tex->numLevels == 1);
    return (tex->baseWidth + tex->_protected->blockWidth - 1) / tex->_protected->blockWidth *
           tex->_protected->blockSize;
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
