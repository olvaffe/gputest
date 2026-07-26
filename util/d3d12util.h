/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef D3D12UTIL_H
#define D3D12UTIL_H

#include "util.h"

#include <dlfcn.h>

#define INITGUID
#include <vkd3d_windows.h>
#define COBJMACROS
#define WIDL_C_INLINE_WRAPPERS
#include <vkd3d_d3d12.h>
#include <vkd3d_device_vkd3d_ext.h>
#include <vkd3d_dxgi.h>

#define LIBVKD3D_PROTON_D3D12_NAME "libvkd3d-proton-d3d12.so"

#define d3d12_log(format, ...) u_log("d3d12", format, ##__VA_ARGS__)
#define d3d12_die(format, ...) u_die("d3d12", format, ##__VA_ARGS__)

struct d3d12_init_params {
    D3D_FEATURE_LEVEL min_feature_level;
};

struct d3d12 {
    struct d3d12_init_params params;

    struct {
        void *handle;

#define PFN_D3D12(type, name) PFN_D3D12_##type name;
#include "d3d12util_entrypoints.inc"
    };

    HRESULT result;

    ID3D12Device *dev;
};

static inline void
d3d12_check(const struct d3d12 *d3d12, const char *msg)
{
    if (FAILED(d3d12->result))
        d3d12_die("%s failed: 0x%08x", msg, (unsigned)d3d12->result);
}

static inline void
d3d12_init_library(struct d3d12 *d3d12)
{
    d3d12->handle = dlopen(LIBVKD3D_PROTON_D3D12_NAME, RTLD_LOCAL | RTLD_LAZY);
    if (!d3d12->handle)
        d3d12_die("failed to load %s: %s", LIBVKD3D_PROTON_D3D12_NAME, dlerror());

#define PFN_D3D12(type, name)                                                                    \
    d3d12->name = (PFN_D3D12_##type)dlsym(d3d12->handle, "D3D12" #name);                         \
    if (!d3d12->name)                                                                            \
        d3d12_die("no D3D12" #name);
#include "d3d12util_entrypoints.inc"
}

static inline void
d3d12_init_dev(struct d3d12 *d3d12)
{
    d3d12->result = d3d12->CreateDevice(NULL, d3d12->params.min_feature_level, &IID_ID3D12Device,
                                        (void **)&d3d12->dev);
    d3d12_check(d3d12, "CreateDevice");
}

static inline void
d3d12_init(struct d3d12 *d3d12, const struct d3d12_init_params *params)
{
    memset(d3d12, 0, sizeof(*d3d12));

    if (params)
        d3d12->params = *params;
    if (!d3d12->params.min_feature_level)
        d3d12->params.min_feature_level = D3D_FEATURE_LEVEL_11_0;

    d3d12_init_library(d3d12);
    d3d12_init_dev(d3d12);
}

static inline void
d3d12_cleanup(struct d3d12 *d3d12)
{
    if (d3d12->dev)
        ID3D12Device_Release(d3d12->dev);
    if (d3d12->handle)
        dlclose(d3d12->handle);
}

#endif /* D3D12UTIL_H */
