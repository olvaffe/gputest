/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "include/gpu/graphite/Context.h"
#include "include/gpu/graphite/ContextOptions.h"
#include "include/gpu/graphite/Recorder.h"
#include "include/gpu/graphite/Recording.h"
#include "include/gpu/graphite/Surface.h"
#include "include/gpu/graphite/vk/VulkanGraphiteContext.h"
#include "skutil.h"
#include "skutil_vk.h"
#include "vkutil.h"

struct canvas_graphite_vk_test {
    uint32_t width;
    uint32_t height;

    struct vk vk;
    struct sk sk;

    std::unique_ptr<sk_vk_backend_context> backend_ctx;
    std::unique_ptr<skgpu::graphite::Context> ctx;

    std::unique_ptr<skgpu::graphite::Recorder> recorder;

    sk_sp<SkSurface> surf;
};

static void
canvas_graphite_vk_test_init(struct canvas_graphite_vk_test *test)
{
    struct vk *vk = &test->vk;
    struct sk *sk = &test->sk;

    const struct vk_init_params vk_params = {
        .enable_all_features = true,
    };
    vk_init(vk, &vk_params);

    sk_init(sk, NULL);

    test->backend_ctx = std::make_unique<sk_vk_backend_context>(vk);
    test->ctx = skgpu::graphite::ContextFactory::MakeVulkan(test->backend_ctx->get(), {});
    if (!test->ctx)
        sk_die("failed to create graphite vk context");

    test->recorder = test->ctx->makeRecorder();
    if (!test->recorder)
        sk_die("failed to create graphite recorder");

    test->surf = sk_create_surface_graphite(sk, test->recorder.get(), test->width, test->height);
}

static void
canvas_graphite_vk_test_cleanup(struct canvas_graphite_vk_test *test)
{
    struct vk *vk = &test->vk;
    struct sk *sk = &test->sk;

    test->surf.reset();
    test->recorder.reset();
    test->ctx.reset();

    sk_cleanup(sk);
    vk_cleanup(vk);
}

static void
canvas_graphite_vk_test_draw(struct canvas_graphite_vk_test *test)
{
    SkCanvas *canvas = test->surf->getCanvas();
    canvas->clear(SkColors::kWhite);

    SkPaint paint;
    paint.setColor4f(SkColors::kRed);
    paint.setAntiAlias(true);
    canvas->drawCircle(test->width / 2, test->height / 2, 30, paint);

    std::unique_ptr<skgpu::graphite::Recording> recording = test->recorder->snap();
    if (!recording)
        sk_die("failed to snap graphite recording");

    skgpu::graphite::InsertRecordingInfo info = {};
    info.fRecording = recording.get();
    test->ctx->insertRecording(info);

    /* this will submit the active cmdbuf after inserting the copy cmd */
    sk_dump_surface_graphite(&test->sk, test->ctx.get(), test->surf, "rt.png");
}

int
main(int argc, const char **argv)
{
    struct canvas_graphite_vk_test test = {
        .width = 300,
        .height = 300,
    };

    canvas_graphite_vk_test_init(&test);
    canvas_graphite_vk_test_draw(&test);
    canvas_graphite_vk_test_cleanup(&test);

    return 0;
}
