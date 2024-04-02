/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "skutil.h"
#include "skutil_vk.h"

struct canvas_ganesh_vk_test {
    uint32_t width;
    uint32_t height;

    struct sk sk;
    struct sk_vk vk;
    sk_sp<GrDirectContext> ctx;
    sk_sp<SkSurface> surf;
};

static void
canvas_ganesh_vk_test_init(struct canvas_ganesh_vk_test *test)
{
    struct sk *sk = &test->sk;
    struct sk_vk *vk = &test->vk;

    sk_init(sk, NULL);
    sk_vk_init(vk);

    const GrVkBackendContext backend = sk_vk_make_backend_context(vk);
    test->ctx = sk_create_context_ganesh_vk(sk, backend);
    test->surf = sk_create_surface_ganesh(sk, test->ctx, test->width, test->height);
}

static void
canvas_ganesh_vk_test_cleanup(struct canvas_ganesh_vk_test *test)
{
    struct sk *sk = &test->sk;
    struct sk_vk *vk = &test->vk;

    test->surf.reset();
    test->ctx.reset();
    sk_vk_cleanup(vk);
    sk_cleanup(sk);
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
