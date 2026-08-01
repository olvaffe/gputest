/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef DXGIUTIL_H
#define DXGIUTIL_H

#include "util.h"

#include <dlfcn.h>

#ifndef FORCEINLINE
#define FORCEINLINE __inline__
#endif

#define INITGUID
#define COBJMACROS
#define WIDL_C_INLINE_WRAPPERS
#include <dxgi1_6.h>
#include <windows.h>

#define LIBDXVK_DXGI_NAME "libdxvk_dxgi.so"

#define dxgi_check(dxgi, msg)                                                                    \
    u_check("DXGI", SUCCEEDED((dxgi)->result), "%s failed: 0x%08x", msg, (unsigned)(dxgi)->result)
#define dxgi_die(format, ...) u_die("DXGI", format __VA_OPT__(, ) __VA_ARGS__)
#define dxgi_log(format, ...) u_log("DXGI", format __VA_OPT__(, ) __VA_ARGS__)

struct dxgi_init_params {
    UINT flags;
    UINT adapter_index;
};

struct dxgi {
    struct dxgi_init_params params;

    struct {
        void *handle;
        HRESULT(WINAPI *CreateDXGIFactory2)(UINT Flags, REFIID riid, void **ppFactory);
        HRESULT(WINAPI *DXGIGetDebugInterface1)(UINT Flags, REFIID riid, void **pDebug);
    };

    HRESULT result;

    IDXGIFactory7 *factory;
    UINT factory_version;

    IDXGIAdapter4 *adapter;
    UINT adapter_version;
};

static inline void
dxgi_init_library(struct dxgi *dxgi)
{
    dxgi->handle = dlopen(LIBDXVK_DXGI_NAME, RTLD_LOCAL | RTLD_LAZY);
    if (!dxgi->handle)
        dxgi_die("failed to load %s: %s", LIBDXVK_DXGI_NAME, dlerror());

    dxgi->CreateDXGIFactory2 =
        (HRESULT(WINAPI *)(UINT, REFIID, void **))dlsym(dxgi->handle, "CreateDXGIFactory2");
    if (!dxgi->CreateDXGIFactory2)
        dxgi_die("no CreateDXGIFactory2");

    dxgi->DXGIGetDebugInterface1 =
        (HRESULT(WINAPI *)(UINT, REFIID, void **))dlsym(dxgi->handle, "DXGIGetDebugInterface1");
    if (!dxgi->DXGIGetDebugInterface1)
        dxgi_die("no DXGIGetDebugInterface1");
}

static inline void
dxgi_init_factory(struct dxgi *dxgi)
{
    const struct {
        UINT version;
        const GUID *iid;
    } versions[] = {
        { 7, &IID_IDXGIFactory7 },
    };

    for (size_t i = 0; i < ARRAY_SIZE(versions); i++) {
        HRESULT hr = dxgi->CreateDXGIFactory2(dxgi->params.flags, versions[i].iid,
                                              (void **)&dxgi->factory);
        if (SUCCEEDED(hr)) {
            dxgi->factory_version = versions[i].version;
            break;
        }
        if (hr != E_NOINTERFACE)
            dxgi_die("CreateDXGIFactory2 failed: 0x%08x", (unsigned)hr);
    }

    if (!dxgi->factory)
        dxgi_die("IDXGIFactory7 required");
}

static inline void
dxgi_init_adapter(struct dxgi *dxgi)
{
    const struct {
        UINT version;
        const GUID *iid;
    } versions[] = {
        { 4, &IID_IDXGIAdapter4 },
    };

    for (size_t i = 0; i < ARRAY_SIZE(versions); i++) {
        HRESULT hr = IDXGIFactory7_EnumAdapterByGpuPreference(
            dxgi->factory, dxgi->params.adapter_index, DXGI_GPU_PREFERENCE_UNSPECIFIED,
            versions[i].iid, (void **)&dxgi->adapter);
        if (SUCCEEDED(hr)) {
            dxgi->adapter_version = versions[i].version;
            break;
        }
        if (hr != E_NOINTERFACE)
            dxgi_die("EnumAdapterByGpuPreference failed: 0x%08x", (unsigned)hr);
    }

    if (!dxgi->adapter)
        dxgi_die("IDXGIAdapter4 required");
}

static inline void
dxgi_init(struct dxgi *dxgi, const struct dxgi_init_params *params)
{
    memset(dxgi, 0, sizeof(*dxgi));

    if (params)
        dxgi->params = *params;

    dxgi_init_library(dxgi);
    dxgi_init_factory(dxgi);
    dxgi_init_adapter(dxgi);
}

static inline void
dxgi_cleanup(struct dxgi *dxgi)
{
    IDXGIAdapter4_Release(dxgi->adapter);
    IDXGIFactory7_Release(dxgi->factory);

    dlclose(dxgi->handle);
}

#endif /* DXGIUTIL_H */
