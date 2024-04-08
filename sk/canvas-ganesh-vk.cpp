/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "skutil.h"
#include "skutil_vk.h"
#include "vkutil.h"

struct canvas_ganesh_vk_test {
    uint32_t width;
    uint32_t height;

    struct vk vk;
    struct sk sk;

    std::unique_ptr<sk_vk_backend_context> backend_ctx;
    sk_sp<GrDirectContext> ctx;
    sk_sp<SkSurface> surf;
};

static void
canvas_ganesh_vk_test_init(struct canvas_ganesh_vk_test *test)
{
    struct vk *vk = &test->vk;
    struct sk *sk = &test->sk;

    vk_init(vk, NULL);
    sk_init(sk, NULL);

    test->backend_ctx = std::make_unique<sk_vk_backend_context>(vk);
    test->ctx = sk_create_context_ganesh_vk(sk, test->backend_ctx->get());
    test->surf = sk_create_surface_ganesh(sk, test->ctx, test->width, test->height);
}

static void
canvas_ganesh_vk_test_cleanup(struct canvas_ganesh_vk_test *test)
{
    struct vk *vk = &test->vk;
    struct sk *sk = &test->sk;

    test->surf.reset();
    test->ctx.reset();
    sk_cleanup(sk);
    vk_cleanup(vk);
}

static void
canvas_ganesh_vk_test_draw(struct canvas_ganesh_vk_test *test)
{
    struct sk *sk = &test->sk;

    SkCanvas *canvas = test->surf->getCanvas();
    canvas->clear(SK_ColorWHITE);

    SkPaint paint;
    paint.setColor(SK_ColorRED);
    paint.setAntiAlias(true);
    canvas->drawCircle(test->width / 2, test->height / 2, 30, paint);

    test->ctx->flushAndSubmit(test->surf.get());

    sk_dump_surface(sk, test->surf, "rt.png");
}

int
main(int argc, const char **argv)
{
    struct canvas_ganesh_vk_test test = {
        .width = 300,
        .height = 300,
    };

    canvas_ganesh_vk_test_init(&test);
    canvas_ganesh_vk_test_draw(&test);
    canvas_ganesh_vk_test_cleanup(&test);

    return 0;
}
