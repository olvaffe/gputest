/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef SKUTIL_H
#define SKUTIL_H

#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkStream.h"
#include "include/core/SkSurface.h"
#include "include/encode/SkPngEncoder.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkImageGanesh.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/vk/GrVkDirectContext.h"
#include "include/gpu/graphite/Context.h"
#include "include/gpu/graphite/Surface.h"
#include "util.h"

#include <memory>

struct sk_init_params {
    int unused;
};

struct sk {
    struct sk_init_params params;
};

#define sk_log(format, ...) u_log("SK", format __VA_OPT__(, ) __VA_ARGS__)
#define sk_die(format, ...) u_die("SK", format __VA_OPT__(, ) __VA_ARGS__)

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
sk_create_context_ganesh_gl(struct sk *sk, sk_sp<const GrGLInterface> gl_interface)
{
    sk_sp<GrDirectContext> ctx = GrDirectContexts::MakeGL(gl_interface);
    if (!ctx)
        sk_die("failed to create ganesh gl context");
    return ctx;
}

static inline sk_sp<GrDirectContext>
sk_create_context_ganesh_vk(struct sk *sk, const skgpu::VulkanBackendContext &backend)
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

static inline sk_sp<SkSurface>
sk_create_surface_graphite(struct sk *sk,
                           skgpu::graphite::Recorder *recorder,
                           uint32_t width,
                           uint32_t height)
{
    const SkImageInfo info = sk_make_image_info(sk, width, height);
    sk_sp<SkSurface> surf = SkSurfaces::RenderTarget(recorder, info);
    if (!surf)
        sk_die("failed to create graphite surface");
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
        if (!surf->readPixels(bitmap.pixmap(), 0, 0))
            sk_die("failed to read pixels from surface");
        pixmap = bitmap.pixmap();
    }

    if (!SkPngEncoder::Encode(&writer, pixmap, SkPngEncoder::Options()))
        sk_die("failed to encode pixmap");
}

static inline void
sk_dump_surface_graphite(struct sk *sk,
                         skgpu::graphite::Context *ctx,
                         sk_sp<SkSurface> surf,
                         const char *filename)
{
    struct graphite_read_ctx {
        bool success;
        SkBitmap bitmap;
        int width;
        int height;
    } read_ctx = { false, {}, surf->width(), surf->height() };

    ctx->asyncRescaleAndReadPixels(
        surf.get(), surf->imageInfo(), SkIRect::MakeWH(surf->width(), surf->height()),
        SkImage::RescaleGamma::kSrc, SkImage::RescaleMode::kNearest,
        [](SkImage::ReadPixelsContext context,
           std::unique_ptr<const SkImage::AsyncReadResult> async_result) {
            auto *r = static_cast<graphite_read_ctx *>(context);
            if (async_result && async_result->count() > 0) {
                r->bitmap.allocPixels(SkImageInfo::MakeN32Premul(r->width, r->height));
                const uint8_t *src = static_cast<const uint8_t *>(async_result->data(0));
                uint8_t *dst = static_cast<uint8_t *>(r->bitmap.getPixels());
                size_t min_row_bytes = std::min(r->bitmap.rowBytes(), async_result->rowBytes(0));
                for (int y = 0; y < r->height; ++y) {
                    memcpy(dst + y * r->bitmap.rowBytes(), src + y * async_result->rowBytes(0),
                           min_row_bytes);
                }
                r->success = true;
            }
        },
        &read_ctx);

    ctx->submit(skgpu::graphite::SyncToCpu::kYes);

    if (!read_ctx.success)
        sk_die("failed to read pixels from graphite surface");

    SkFILEWStream writer(filename);
    if (!writer.isValid())
        sk_die("failed to create %s", filename);

    if (!SkPngEncoder::Encode(&writer, read_ctx.bitmap.pixmap(), SkPngEncoder::Options()))
        sk_die("failed to encode png");
}

static inline sk_sp<SkImage>
sk_load_png(struct sk *sk, const char *filename)
{
    sk_sp<SkData> data = SkData::MakeFromFileName(filename);
    if (!data)
        sk_die("failed to read %s", filename);

    sk_sp<SkImage> img = SkImages::DeferredFromEncodedData(std::move(data));
    if (!img)
        sk_die("failed to decode png %s", filename);

    return img;
}

#endif /* SKUTIL_H */
