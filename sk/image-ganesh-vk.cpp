/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "skutil.h"
#include "skutil_vk.h"
#include "vkutil.h"

struct image_ganesh_vk_test {
    bool upload;
    const char *filename;

    struct vk vk;
    struct sk sk;

    std::unique_ptr<sk_vk_backend_context> backend_ctx;
    sk_sp<GrDirectContext> ctx;

    sk_sp<SkImage> img;
    sk_sp<SkSurface> surf;
};

static void
image_ganesh_vk_test_init(struct image_ganesh_vk_test *test)
{
    struct vk *vk = &test->vk;
    struct sk *sk = &test->sk;

    vk_init(vk, NULL);
    sk_init(sk, NULL);

    test->backend_ctx = std::make_unique<sk_vk_backend_context>(vk);
    test->ctx = sk_create_context_ganesh_vk(sk, test->backend_ctx->get());

    test->img = sk_load_png(sk, test->filename);
    assert(!test->img->isTextureBacked());
    if (test->upload) {
        SkPixmap pixmap;
        test->img->peekPixels(&pixmap);

        GrBackendTexture tex = test->ctx->createBackendTexture(
            pixmap, kTopLeft_GrSurfaceOrigin, GrRenderable::kNo, GrProtected::kNo);
        if (!tex.isValid())
            sk_die("failed to create backend texture");
        test->img = SkImages::AdoptTextureFrom(test->ctx.get(), tex, kTopLeft_GrSurfaceOrigin,
                                               test->img->colorType());

        assert(test->img->isTextureBacked());
    }

    test->surf = sk_create_surface_ganesh(sk, test->ctx, test->img->width(), test->img->height());
}

static void
image_ganesh_vk_test_cleanup(struct image_ganesh_vk_test *test)
{
    struct vk *vk = &test->vk;
    struct sk *sk = &test->sk;

    test->surf.reset();
    test->img.reset();
    test->ctx.reset();
    sk_cleanup(sk);
    vk_cleanup(vk);
}

static void
image_ganesh_vk_test_draw(struct image_ganesh_vk_test *test)
{
    struct sk *sk = &test->sk;

    SkCanvas *canvas = test->surf->getCanvas();
    canvas->clear(SK_ColorWHITE);

    canvas->drawImage(test->img, 0, 0);

    test->ctx->flushAndSubmit(test->surf.get());

    sk_dump_surface(sk, test->surf, "rt.png");
}

int
main(int argc, const char **argv)
{
    struct image_ganesh_vk_test test = {
        .upload = true,
    };

    if (argc != 2)
        sk_die("usage: %s <png-file>", argv[0]);

    test.filename = argv[1];

    image_ganesh_vk_test_init(&test);
    image_ganesh_vk_test_draw(&test);
    image_ganesh_vk_test_cleanup(&test);

    return 0;
}
