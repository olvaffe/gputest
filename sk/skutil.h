/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef SKUTIL_H
#define SKUTIL_H

#include "include/codec/SkPngDecoder.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkStream.h"
#include "include/core/SkSurface.h"
#include "include/encode/SkPngEncoder.h"
#include "include/gpu/GrBackendSurface.h"
#include "include/gpu/GrDirectContext.h"
#include "include/gpu/ganesh/SkImageGanesh.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/vk/GrVkDirectContext.h"

#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <memory>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PRINTFLIKE(f, a) __attribute__((format(printf, f, a)))
#define NORETURN __attribute__((noreturn))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

struct sk_init_params {
    int unused;
};

struct sk {
    struct sk_init_params params;
};

static inline void
sk_logv(const char *format, va_list ap)
{
    printf("SK: ");
    vprintf(format, ap);
    printf("\n");
}

static inline void NORETURN
sk_diev(const char *format, va_list ap)
{
    sk_logv(format, ap);
    abort();
}

static inline void PRINTFLIKE(1, 2) sk_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    sk_logv(format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(1, 2) NORETURN sk_die(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    sk_diev(format, ap);
    va_end(ap);
}

static inline void
sk_init(struct sk *sk, const struct sk_init_params *params)
{
    memset(sk, 0, sizeof(*sk));
    if (params)
        sk->params = *params;
}

static inline void
sk_cleanup(struct sk *sk)
{
}

static inline SkImageInfo
sk_make_image_info(struct sk *sk, uint32_t width, uint32_t height)
{
    const SkColorType color_type = kRGBA_8888_SkColorType;
    const SkAlphaType alpha_type = kPremul_SkAlphaType;
    return SkImageInfo::Make(width, height, color_type, alpha_type);
}

static inline sk_sp<SkSurface>
sk_create_surface_raster(struct sk *sk, uint32_t width, uint32_t height)
{
    const SkImageInfo info = sk_make_image_info(sk, width, height);
    sk_sp<SkSurface> surf = SkSurfaces::Raster(info);
    if (!surf)
        sk_die("failed to create raster surface");
    return surf;
}

static inline sk_sp<GrDirectContext>
sk_create_context_ganesh_gl(struct sk *sk)
{
    /* use the default GrGLInterface and GrContextOptions */
    sk_sp<GrDirectContext> ctx = GrDirectContexts::MakeGL();
    if (!ctx)
        sk_die("failed to create ganesh gl context");
    return ctx;
}

static inline sk_sp<GrDirectContext>
sk_create_context_ganesh_vk(struct sk *sk, const GrVkBackendContext &backend)
{
    /* use the default GrContextOptions */
    sk_sp<GrDirectContext> ctx = GrDirectContexts::MakeVulkan(backend);
    if (!ctx)
        sk_die("failed to create ganesh vk context");
    return ctx;
}

static inline sk_sp<SkSurface>
sk_create_surface_ganesh(struct sk *sk,
                         sk_sp<GrDirectContext> ctx,
                         uint32_t width,
                         uint32_t height)
{
    const SkImageInfo info = sk_make_image_info(sk, width, height);
    sk_sp<SkSurface> surf = SkSurfaces::RenderTarget(ctx.get(), skgpu::Budgeted::kYes, info);
    if (!surf)
        sk_die("failed to create ganesh surface");
    return surf;
}

static inline void
sk_dump_surface(struct sk *sk, sk_sp<SkSurface> surf, const char *filename)
{
    SkFILEWStream writer(filename);
    if (!writer.isValid())
        sk_die("failed to create %s", filename);

    SkBitmap bitmap;
    SkPixmap pixmap;
    if (!surf->peekPixels(&pixmap)) {
        bitmap.allocPixels(surf->imageInfo());
        surf->readPixels(bitmap.pixmap(), 0, 0);
        pixmap = bitmap.pixmap();
    }

    if (!SkPngEncoder::Encode(&writer, pixmap, SkPngEncoder::Options()))
        sk_die("failed to encode pixmap");
}

static inline sk_sp<SkImage>
sk_load_png(struct sk *sk, const char *filename)
{
    std::unique_ptr<SkFILEStream> reader = SkFILEStream::Make(filename);
    if (!reader)
        sk_die("failed to open %s", filename);

    std::unique_ptr<SkCodec> codec = SkPngDecoder::Decode(std::move(reader), NULL);

    return std::get<0>(codec->getImage());
}

#endif /* SKUTIL_H */
