/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef SKUTIL_EGL_H
#define SKUTIL_EGL_H

#include "include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "eglutil.h"
#include "skutil.h"

static inline GrGLFuncPtr
sk_egl_get_proc(void *ctx, const char name[])
{
    struct egl *egl = (struct egl *)ctx;
    return (GrGLFuncPtr)egl->GetProcAddress(name);
}

static inline sk_sp<GrDirectContext>
sk_create_context_ganesh_gl(struct sk *sk, struct egl *egl)
{
    sk_sp<const GrGLInterface> gl_interface;
    if (egl)
        gl_interface = GrGLMakeAssembledGLESInterface(egl, sk_egl_get_proc);
    if (!gl_interface)
        gl_interface = GrGLMakeNativeInterface();

    sk_sp<GrDirectContext> ctx = GrDirectContexts::MakeGL(gl_interface);
    if (!ctx)
        sk_die("failed to create ganesh gl context");
    return ctx;
}

#endif /* SKUTIL_EGL_H */
