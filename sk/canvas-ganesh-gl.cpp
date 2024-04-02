/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "skutil.h"
#include "skutil_egl.h"

struct canvas_ganesh_gl_test {
    uint32_t width;
    uint32_t height;

    struct sk sk;
    struct sk_egl egl;
    sk_sp<GrDirectContext> ctx;
    sk_sp<SkSurface> surf;
};

static void
canvas_ganesh_gl_test_init(struct canvas_ganesh_gl_test *test)
{
    struct sk *sk = &test->sk;
    struct sk_egl *egl = &test->egl;

    sk_init(sk, NULL);
    sk_egl_init(egl);

    test->ctx = sk_create_context_ganesh_gl(sk);
    test->surf = sk_create_surface_ganesh(sk, test->ctx, test->width, test->height);
}

static void
canvas_ganesh_gl_test_cleanup(struct canvas_ganesh_gl_test *test)
{
    struct sk *sk = &test->sk;
    struct sk_egl *egl = &test->egl;

    test->surf.reset();
    test->ctx.reset();
    sk_egl_cleanup(egl);
    sk_cleanup(sk);
}

static void
canvas_ganesh_gl_test_draw(struct canvas_ganesh_gl_test *test)
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
    struct canvas_ganesh_gl_test test = {
        .width = 300,
        .height = 300,
    };

    canvas_ganesh_gl_test_init(&test);
    canvas_ganesh_gl_test_draw(&test);
    canvas_ganesh_gl_test_cleanup(&test);

    return 0;
}
