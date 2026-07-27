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

#define d3d11_log(format, ...) u_log("D3D11", format, ##__VA_ARGS__)
#define d3d11_die(format, ...) u_die("D3D11", format, ##__VA_ARGS__)

struct d3d11_init_params {
    IDXGIAdapter *adapter;
    UINT flags;
    const D3D_FEATURE_LEVEL *feature_levels;
    UINT feature_level_count;
};

struct d3d11 {
    struct d3d11_init_params params;

    struct {
        void *handle;
        PFN_D3D11_CREATE_DEVICE CreateDevice;
    };

    HRESULT result;

    ID3D11Device *dev;
    UINT dev_version;
    D3D_FEATURE_LEVEL feature_level;

    ID3D11DeviceContext *ctx;
};

static inline void
d3d11_check(const struct d3d11 *d3d11, const char *msg)
{
    if (FAILED(d3d11->result))
        d3d11_die("%s failed: 0x%08x", msg, (unsigned)d3d11->result);
}

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

    ID3D11Device *dev = NULL;
    d3d11->result =
        d3d11->CreateDevice(d3d11->params.adapter, driver_type, NULL, d3d11->params.flags,
                            d3d11->params.feature_levels, d3d11->params.feature_level_count,
                            D3D11_SDK_VERSION, &dev, &d3d11->feature_level, &d3d11->ctx);
    d3d11_check(d3d11, "D3D11CreateDevice");

    const struct {
        const GUID *iid;
        UINT version;
    } versions[] = {
        { &IID_ID3D11Device5, 5 }, { &IID_ID3D11Device4, 4 }, { &IID_ID3D11Device3, 3 },
        { &IID_ID3D11Device2, 2 }, { &IID_ID3D11Device1, 1 },
    };

    for (size_t i = 0; i < ARRAY_SIZE(versions); i++) {
        if (SUCCEEDED(ID3D11Device_QueryInterface(dev, versions[i].iid, (void **)&d3d11->dev))) {
            d3d11->dev_version = versions[i].version;
            break;
        }
    }

    if (d3d11->dev) {
        ID3D11Device_Release(dev);
    } else {
        d3d11->dev = dev;
    }
}

static inline void
d3d11_init(struct d3d11 *d3d11, const struct d3d11_init_params *params)
{
    memset(d3d11, 0, sizeof(*d3d11));

    if (params)
        d3d11->params = *params;

    d3d11_init_library(d3d11);
    d3d11_init_dev(d3d11);
}

static inline void
d3d11_cleanup(struct d3d11 *d3d11)
{
    ID3D11DeviceContext_Release(d3d11->ctx);
    ID3D11Device_Release(d3d11->dev);

    dlclose(d3d11->handle);
}

#endif /* D3D11UTIL_H */
