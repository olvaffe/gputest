/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef D3D11UTIL_H
#define D3D11UTIL_H

#include "util.h"

#include <dlfcn.h>

#ifndef FORCEINLINE
#define FORCEINLINE __inline__
#endif

#define INITGUID
#define COBJMACROS
#define WIDL_C_INLINE_WRAPPERS
#include <d3d11_4.h>
#include <windows.h>

#define LIBDXVK_D3D11_NAME "libdxvk_d3d11.so"

#define d3d11_log(format, ...) u_log("D3D11", format __VA_OPT__(, ) __VA_ARGS__)
#define d3d11_die(format, ...) u_die("D3D11", format __VA_OPT__(, ) __VA_ARGS__)

struct d3d11_init_params {
    IDXGIAdapter *adapter;
    UINT flags;
    D3D_FEATURE_LEVEL feature_level;
};

struct d3d11 {
    struct d3d11_init_params params;

    struct {
        void *handle;
        PFN_D3D11_CREATE_DEVICE CreateDevice;
    };

    HRESULT result;

    ID3D11Device5 *dev;
    UINT dev_version;
    D3D_FEATURE_LEVEL feature_level;

    ID3D11DeviceContext4 *ctx;
    UINT ctx_version;
};

#define d3d11_check(d3d11, msg)                                                                  \
    u_check("D3D11", SUCCEEDED((d3d11)->result), "%s failed: 0x%08x", msg,                       \
            (unsigned)(d3d11)->result)

static inline void
d3d11_init_library(struct d3d11 *d3d11)
{
    d3d11->handle = dlopen(LIBDXVK_D3D11_NAME, RTLD_LOCAL | RTLD_LAZY);
    if (!d3d11->handle)
        d3d11_die("failed to load %s: %s", LIBDXVK_D3D11_NAME, dlerror());

    d3d11->CreateDevice = (PFN_D3D11_CREATE_DEVICE)dlsym(d3d11->handle, "D3D11CreateDevice");
    if (!d3d11->CreateDevice)
        d3d11_die("no D3D11CreateDevice");
}

static inline void
d3d11_init_dev(struct d3d11 *d3d11)
{
    D3D_DRIVER_TYPE driver_type =
        d3d11->params.adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;

    const D3D_FEATURE_LEVEL *feature_levels = NULL;
    UINT feature_level_count = 0;
    if (d3d11->params.feature_level) {
        feature_levels = &d3d11->params.feature_level;
        feature_level_count = 1;
    }

    ID3D11Device *dev = NULL;
    d3d11->result = d3d11->CreateDevice(d3d11->params.adapter, driver_type, NULL,
                                        d3d11->params.flags, feature_levels, feature_level_count,
                                        D3D11_SDK_VERSION, &dev, &d3d11->feature_level, NULL);
    d3d11_check(d3d11, "D3D11CreateDevice");

    const struct {
        const GUID *iid;
        UINT version;
    } versions[] = {
        { &IID_ID3D11Device5, 5 },
    };

    for (size_t i = 0; i < ARRAY_SIZE(versions); i++) {
        HRESULT hr = ID3D11Device_QueryInterface(dev, versions[i].iid, (void **)&d3d11->dev);
        if (SUCCEEDED(hr)) {
            d3d11->dev_version = versions[i].version;
            break;
        }
        if (hr != E_NOINTERFACE)
            d3d11_die("ID3D11Device_QueryInterface failed: 0x%08x", (unsigned)hr);
    }
    if (!d3d11->dev)
        d3d11_die("ID3D11Device5 required");

    ID3D11Device_Release(dev);
}

static inline void
d3d11_init_ctx(struct d3d11 *d3d11)
{
    ID3D11DeviceContext *ctx = NULL;
    ID3D11Device5_GetImmediateContext(d3d11->dev, &ctx);
    if (!ctx)
        d3d11_die("GetImmediateContext failed");

    const struct {
        const GUID *iid;
        UINT version;
    } versions[] = {
        { &IID_ID3D11DeviceContext4, 4 },
    };

    for (size_t i = 0; i < ARRAY_SIZE(versions); i++) {
        HRESULT hr =
            ID3D11DeviceContext_QueryInterface(ctx, versions[i].iid, (void **)&d3d11->ctx);
        if (SUCCEEDED(hr)) {
            d3d11->ctx_version = versions[i].version;
            break;
        }
        if (hr != E_NOINTERFACE)
            d3d11_die("ID3D11DeviceContext_QueryInterface failed: 0x%08x", (unsigned)hr);
    }
    if (!d3d11->ctx)
        d3d11_die("ID3D11DeviceContext4 required");

    ID3D11DeviceContext_Release(ctx);
}

static inline void
d3d11_init(struct d3d11 *d3d11, const struct d3d11_init_params *params)
{
    memset(d3d11, 0, sizeof(*d3d11));

    if (params)
        d3d11->params = *params;

    d3d11_init_library(d3d11);
    d3d11_init_dev(d3d11);
    d3d11_init_ctx(d3d11);
}

static inline void
d3d11_cleanup(struct d3d11 *d3d11)
{
    ID3D11DeviceContext4_Release(d3d11->ctx);
    ID3D11Device5_Release(d3d11->dev);

    dlclose(d3d11->handle);
}

static inline ID3D11Texture2D *
d3d11_create_tex_from_ppm(struct d3d11 *d3d11, const void *ppm_data, size_t ppm_size)
{
    uint32_t width;
    uint32_t height;
    const void *ppm_pixels = u_parse_ppm(ppm_data, ppm_size, &width, &height);

    void *rgba_pixels = malloc(width * height * 4);
    if (!rgba_pixels)
        d3d11_die("failed to allocate rgba pixels");

    const struct u_format_conversion conv = {
        .width = width,
        .height = height,
        .src_format = DRM_FORMAT_BGR888,
        .src_plane_count = 1,
        .src_plane_ptrs = { ppm_pixels },
        .src_plane_strides = { width * 3 },
        .dst_format = DRM_FORMAT_ABGR8888,
        .dst_plane_count = 1,
        .dst_plane_ptrs = { rgba_pixels },
        .dst_plane_strides = { width * 4 },
    };
    u_convert_format(&conv);

    const D3D11_TEXTURE2D_DESC tex_desc = {
        .Width = width,
        .Height = height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = {
            .Count = 1,
            .Quality = 0,
        },
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
    };
    const D3D11_SUBRESOURCE_DATA sub_data = {
        .pSysMem = rgba_pixels,
        .SysMemPitch = width * 4,
    };

    ID3D11Texture2D *tex = NULL;
    d3d11->result = ID3D11Device5_CreateTexture2D(d3d11->dev, &tex_desc, &sub_data, &tex);
    d3d11_check(d3d11, "CreateTexture2D (Texture)");

    free(rgba_pixels);

    return tex;
}

static inline void
d3d11_dump_image(struct d3d11 *d3d11, ID3D11Texture2D *tex, const char *filename)
{
    D3D11_TEXTURE2D_DESC desc;
    ID3D11Texture2D_GetDesc(tex, &desc);

    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;

    ID3D11Texture2D *staging = NULL;
    d3d11->result = ID3D11Device5_CreateTexture2D(d3d11->dev, &desc, NULL, &staging);
    d3d11_check(d3d11, "CreateTexture2D (Staging)");

    ID3D11DeviceContext4_CopyResource(d3d11->ctx, (ID3D11Resource *)staging,
                                      (ID3D11Resource *)tex);

    D3D11_MAPPED_SUBRESOURCE mapped;
    d3d11->result = ID3D11DeviceContext4_Map(d3d11->ctx, (ID3D11Resource *)staging, 0,
                                             D3D11_MAP_READ, 0, &mapped);
    d3d11_check(d3d11, "Map Staging Texture");

    u_write_ppm(filename, mapped.pData, desc.Width, desc.Height, mapped.RowPitch);

    ID3D11DeviceContext4_Unmap(d3d11->ctx, (ID3D11Resource *)staging, 0);
    ID3D11Texture2D_Release(staging);
}

#endif /* D3D11UTIL_H */
