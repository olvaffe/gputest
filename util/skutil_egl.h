/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef SKUTIL_EGL_H
#define SKUTIL_EGL_H

#include "eglutil.h"
#include "include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "skutil.h"

static inline GrGLFuncPtr
sk_egl_get_proc(void *ctx, const char name[])
{
    struct egl *egl = (struct egl *)ctx;
    return (GrGLFuncPtr)egl->GetProcAddress(name);
}

static inline sk_sp<const GrGLInterface>
sk_egl_create_gl_interface(struct egl *egl)
{
    return GrGLMakeAssembledGLESInterface(egl, sk_egl_get_proc);
}

#endif /* SKUTIL_EGL_H */
