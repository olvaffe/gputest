/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef SKUTIL_EGL_H
#define SKUTIL_EGL_H

#include "skutil.h"

#define EGL_EGL_PROTOTYPES 0
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <dlfcn.h>

struct sk_egl {
    void *handle;
    PFNEGLGETPROCADDRESSPROC GetProcAddress;
    PFNEGLQUERYSTRINGPROC QueryString;
    PFNEGLQUERYDEVICESEXTPROC QueryDevicesEXT;
    PFNEGLQUERYDEVICESTRINGEXTPROC QueryDeviceStringEXT;
    PFNEGLGETPLATFORMDISPLAYPROC GetPlatformDisplay;
    PFNEGLINITIALIZEPROC Initialize;
    PFNEGLQUERYAPIPROC QueryAPI;
    PFNEGLCREATECONTEXTPROC CreateContext;
    PFNEGLMAKECURRENTPROC MakeCurrent;
    PFNEGLDESTROYCONTEXTPROC DestroyContext;
    PFNEGLTERMINATEPROC Terminate;
    PFNEGLRELEASETHREADPROC ReleaseThread;

    EGLDeviceEXT dev;
    EGLDisplay dpy;

    EGLContext ctx;
};

static inline void
sk_egl_init_library(struct sk_egl *egl)
{
    const char libegl_name[] = "libEGL.so.1";
    egl->handle = dlopen(libegl_name, RTLD_LOCAL | RTLD_LAZY);
    if (!egl->handle)
        sk_die("failed to load %s: %s", libegl_name, dlerror());

    const char gipa_name[] = "eglGetProcAddress";
    egl->GetProcAddress = (PFNEGLGETPROCADDRESSPROC)dlsym(egl->handle, gipa_name);
    if (!egl->GetProcAddress)
        sk_die("failed to find %s: %s", gipa_name, dlerror());

#define GPA(proc, name)                                                                          \
    do {                                                                                         \
        egl->name = (PFNEGL##proc##PROC)egl->GetProcAddress("egl" #name);                        \
        if (!egl->name)                                                                          \
            sk_die("failed to find egl" #name);                                                  \
    } while (false)
    GPA(QUERYSTRING, QueryString);
    GPA(QUERYDEVICESEXT, QueryDevicesEXT);
    GPA(QUERYDEVICESTRINGEXT, QueryDeviceStringEXT);
    GPA(GETPLATFORMDISPLAY, GetPlatformDisplay);
    GPA(INITIALIZE, Initialize);
    GPA(QUERYAPI, QueryAPI);
    GPA(CREATECONTEXT, CreateContext);
    GPA(MAKECURRENT, MakeCurrent);
    GPA(DESTROYCONTEXT, DestroyContext);
    GPA(TERMINATE, Terminate);
    GPA(RELEASETHREAD, ReleaseThread);
#undef GPA
}

static inline void
sk_egl_init_display(struct sk_egl *egl)
{
    const char *client_exts = egl->QueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (!client_exts || !strstr(client_exts, "EGL_EXT_device_enumeration") ||
        !strstr(client_exts, "EGL_EXT_device_query") ||
        !strstr(client_exts, "EGL_EXT_platform_device"))
        sk_die("no EGL platform device support");

    EGLDeviceEXT devs[16];
    EGLint count;
    if (!egl->QueryDevicesEXT(ARRAY_SIZE(devs), devs, &count))
        sk_die("failed to query devices");

    egl->dev = EGL_NO_DEVICE_EXT;
    for (int i = 0; i < count; i++) {
        const char *exts = egl->QueryDeviceStringEXT(devs[i], EGL_EXTENSIONS);

        /* EGL_EXT_device_drm_render_node */
        if (!strstr(exts, "EGL_EXT_device_drm_render_node"))
            continue;

        const bool swrast = strstr(exts, "software");
        if (!swrast || true) {
            egl->dev = devs[i];
            break;
        }
    }
    if (egl->dev == EGL_NO_DEVICE_EXT)
        sk_die("failed to find a hw rendernode device");

    egl->dpy = egl->GetPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, egl->dev, NULL);
    if (egl->dpy == EGL_NO_DISPLAY)
        sk_die("failed to get platform display");

    EGLint major;
    EGLint minor;
    if (!egl->Initialize(egl->dpy, &major, &minor))
        sk_die("failed to initialize display");
    if (major != 1 || minor < 5)
        sk_die("EGL 1.5 is required");

    const char *dpy_exts = egl->QueryString(egl->dpy, EGL_EXTENSIONS);
    if (!strstr(dpy_exts, "EGL_KHR_no_config_context"))
        sk_die("missing EGL_KHR_no_config_context");
}

static inline void
sk_egl_init_context(struct sk_egl *egl)
{
    if (egl->QueryAPI() != EGL_OPENGL_ES_API)
        sk_die("current api is not GLES");

    const EGLint ctx_attrs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE,
    };

    EGLContext ctx = egl->CreateContext(egl->dpy, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, ctx_attrs);
    if (ctx == EGL_NO_CONTEXT)
        sk_die("failed to create a context");

    if (!egl->MakeCurrent(egl->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx))
        sk_die("failed to make context current");

    egl->ctx = ctx;
}

static inline void
sk_egl_init(struct sk_egl *egl)
{
    memset(egl, 0, sizeof(*egl));

    sk_egl_init_library(egl);
    sk_egl_init_display(egl);
    sk_egl_init_context(egl);
}

static inline void
sk_egl_cleanup(struct sk_egl *egl)
{
    egl->MakeCurrent(egl->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    egl->DestroyContext(egl->dpy, egl->ctx);

    egl->Terminate(egl->dpy);
    egl->ReleaseThread();

    dlclose(egl->handle);
}

#endif /* SKUTIL_EGL_H */
