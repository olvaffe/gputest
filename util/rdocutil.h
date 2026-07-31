/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef RDOCUTIL_H
#define RDOCUTIL_H

#include "util.h"

#include <dlfcn.h>
#include <renderdoc_app.h>

#define LIBRENDERDOC_NAME "librenderdoc.so"

#define rdoc_log(format, ...) u_log("RDOC", format __VA_OPT__(, ) __VA_ARGS__)

struct rdoc {
    RENDERDOC_API_1_7_0 *api;
};

static inline void
rdoc_init(struct rdoc *rdoc)
{
    const char get_api_name[] = "RENDERDOC_GetAPI";

    memset(rdoc, 0, sizeof(*rdoc));

    pRENDERDOC_GetAPI get_api = dlsym(RTLD_DEFAULT, get_api_name);
    if (!get_api) {
        void *handle = dlopen(LIBRENDERDOC_NAME, RTLD_NOLOAD | RTLD_LAZY);
        if (handle) {
            get_api = dlsym(handle, get_api_name);
            dlclose(handle);
        }
    }
    if (!get_api)
        return;

    if (!get_api(eRENDERDOC_API_Version_1_7_0, (void **)&rdoc->api))
        rdoc->api = NULL;
}

static inline void
rdoc_cleanup(struct rdoc *rdoc)
{
}

static inline void
rdoc_start(struct rdoc *rdoc)
{
    if (!rdoc->api)
        return;

    rdoc->api->StartFrameCapture(NULL, NULL);
}

static inline void
rdoc_end(struct rdoc *rdoc)
{
    if (!rdoc->api)
        return;

    if (rdoc->api->EndFrameCapture(NULL, NULL)) {
        rdoc_log("frame captured with template %s", rdoc->api->GetCaptureFilePathTemplate());
    } else {
        rdoc_log("frame capture failed");
    }
}

#endif /* RDOCUTIL_H */
