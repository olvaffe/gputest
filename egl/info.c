/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "eglutil.h"

#ifndef GL_REQUESTABLE_EXTENSIONS_ANGLE
#define GL_REQUESTABLE_EXTENSIONS_ANGLE 0x93A8
#endif

int
main(void)
{
    struct egl egl;

    egl_init(&egl, NULL);

    egl_log("client EGL_EXTENSIONS: %s", egl.QueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS));
    if (egl.dev != EGL_NO_DEVICE_EXT)
        egl_log("device EGL_EXTENSIONS: %s", egl.QueryDeviceStringEXT(egl.dev, EGL_EXTENSIONS));
    egl_log("--");
    egl_log("EGL_VENDOR: %s", egl.QueryString(egl.dpy, EGL_VENDOR));
    egl_log("EGL_VERSION: %s", egl.QueryString(egl.dpy, EGL_VERSION));
    egl_log("EGL_CLIENT_APIS: %s", egl.QueryString(egl.dpy, EGL_CLIENT_APIS));
    egl_log("EGL_EXTENSIONS: %s", egl.QueryString(egl.dpy, EGL_EXTENSIONS));
    egl_log("--");
    egl_log("GL_VENDOR: %s", egl.gl.GetString(GL_VENDOR));
    egl_log("GL_RENDERER: %s", egl.gl.GetString(GL_RENDERER));
    egl_log("GL_VERSION: %s", egl.gl.GetString(GL_VERSION));
    egl_log("GL_SHADING_LANGUAGE_VERSION: %s", egl.gl.GetString(GL_SHADING_LANGUAGE_VERSION));

    const char *exts = (const char *)egl.gl.GetString(GL_EXTENSIONS);
    egl_log("GL_EXTENSIONS: %s", exts);
    if (strstr(exts, "GL_ANGLE_request_extension")) {
        egl_log("GL_REQUESTABLE_EXTENSIONS_ANGLE: %s",
                egl.gl.GetString(GL_REQUESTABLE_EXTENSIONS_ANGLE));
    }

    egl_cleanup(&egl);

    return 0;
}
