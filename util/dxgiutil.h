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

#define dxgi_log(format, ...) u_log("DXGI", format, ##__VA_ARGS__)
#define dxgi_die(format, ...) u_die("DXGI", format, ##__VA_ARGS__)

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

    IDXGIFactory1 *factory;
    UINT factory_version;

    IDXGIAdapter1 *adapter;
    UINT adapter_version;
};

static inline void
dxgi_check(const struct dxgi *dxgi, const char *msg)
{
    if (FAILED(dxgi->result))
        dxgi_die("%s failed: 0x%08x", msg, (unsigned)dxgi->result);
}

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
        { 7, &IID_IDXGIFactory7 }, { 6, &IID_IDXGIFactory6 }, { 5, &IID_IDXGIFactory5 },
        { 4, &IID_IDXGIFactory4 }, { 3, &IID_IDXGIFactory3 }, { 2, &IID_IDXGIFactory2 },
        { 1, &IID_IDXGIFactory1 },
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
        dxgi_die("no IDXGIFactory1");
}

static inline void
dxgi_init_adapter(struct dxgi *dxgi)
{
    IDXGIAdapter1 *adapter1 = NULL;
    dxgi->result =
        IDXGIFactory1_EnumAdapters1(dxgi->factory, dxgi->params.adapter_index, &adapter1);
    dxgi_check(dxgi, "EnumAdapters1");

    const struct {
        UINT version;
        const GUID *iid;
    } versions[] = {
        { 4, &IID_IDXGIAdapter4 },
        { 3, &IID_IDXGIAdapter3 },
        { 2, &IID_IDXGIAdapter2 },
    };

    for (size_t i = 0; i < ARRAY_SIZE(versions); i++) {
        HRESULT hr =
            IDXGIAdapter1_QueryInterface(adapter1, versions[i].iid, (void **)&dxgi->adapter);
        if (SUCCEEDED(hr)) {
            dxgi->adapter_version = versions[i].version;
            break;
        }
        if (hr != E_NOINTERFACE)
            dxgi_die("IDXGIAdapter1_QueryInterface failed: 0x%08x", (unsigned)hr);
    }

    if (dxgi->adapter) {
        IDXGIAdapter1_Release(adapter1);
    } else {
        dxgi->adapter = adapter1;
        dxgi->adapter_version = 1;
    }
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
    IDXGIAdapter1_Release(dxgi->adapter);
    IDXGIFactory1_Release(dxgi->factory);

    dlclose(dxgi->handle);
}

#endif /* DXGIUTIL_H */
