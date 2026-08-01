/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "include/gpu/graphite/Context.h"
#include "include/gpu/graphite/ContextOptions.h"
#include "include/gpu/graphite/Image.h"
#include "include/gpu/graphite/Recorder.h"
#include "include/gpu/graphite/Recording.h"
#include "include/gpu/graphite/Surface.h"
#include "include/gpu/graphite/vk/VulkanGraphiteContext.h"
#include "skutil.h"
#include "skutil_vk.h"
#include "vkutil.h"

struct image_graphite_vk_test {
    bool upload;
    const char *filename;

    struct vk vk;
    struct sk sk;

    std::unique_ptr<sk_vk_backend_context> backend_ctx;
    std::unique_ptr<skgpu::graphite::Context> ctx;

    std::unique_ptr<skgpu::graphite::Recorder> recorder;

    sk_sp<SkImage> img;
    sk_sp<SkSurface> surf;
};

static void
image_graphite_vk_test_init(struct image_graphite_vk_test *test)
{
    struct vk *vk = &test->vk;
    struct sk *sk = &test->sk;

    const struct vk_init_params vk_params = {
        .api_version = VK_API_VERSION_1_4,
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

    test->img = sk_load_png(sk, test->filename);
    assert(!test->img->isTextureBacked());
    if (test->upload) {
        test->img = SkImages::TextureFromImage(test->recorder.get(), test->img.get());
        if (!test->img)
            sk_die("failed to upload texture to GPU");

        assert(test->img->isTextureBacked());
    }

    test->surf = sk_create_surface_graphite(sk, test->recorder.get(), test->img->width(),
                                            test->img->height());
}

static void
image_graphite_vk_test_cleanup(struct image_graphite_vk_test *test)
{
    struct vk *vk = &test->vk;
    struct sk *sk = &test->sk;

    test->surf.reset();
    test->img.reset();
    test->recorder.reset();
    test->ctx.reset();

    sk_cleanup(sk);
    vk_cleanup(vk);
}

static void
image_graphite_vk_test_draw(struct image_graphite_vk_test *test)
{
    SkCanvas *canvas = test->surf->getCanvas();
    canvas->clear(SkColors::kWhite);

    canvas->drawImage(test->img, 0, 0);

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
    struct image_graphite_vk_test test = {
        .upload = true,
    };

    if (argc != 2)
        sk_die("usage: %s <png-file>", argv[0]);

    test.filename = argv[1];

    image_graphite_vk_test_init(&test);
    image_graphite_vk_test_draw(&test);
    image_graphite_vk_test_cleanup(&test);

    return 0;
}
