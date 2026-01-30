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

struct rdoc {
    RENDERDOC_API_1_0_0 *api;
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

    if (!get_api(eRENDERDOC_API_Version_1_0_0, (void **)&rdoc->api))
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

    rdoc->api->EndFrameCapture(NULL, NULL);
}

#endif /* RDOCUTIL_H */
