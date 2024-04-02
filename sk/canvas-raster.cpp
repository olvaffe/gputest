/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "skutil.h"

struct canvas_raster_test {
    uint32_t width;
    uint32_t height;

    struct sk sk;
    sk_sp<SkSurface> surf;
};

static void
canvas_raster_test_init(struct canvas_raster_test *test)
{
    struct sk *sk = &test->sk;

    sk_init(sk, NULL);
    test->surf = sk_create_surface_raster(sk, test->width, test->height);
}

static void
canvas_raster_test_cleanup(struct canvas_raster_test *test)
{
    struct sk *sk = &test->sk;

    test->surf.reset();
    sk_cleanup(sk);
}

static void
canvas_raster_test_draw(struct canvas_raster_test *test)
{
    struct sk *sk = &test->sk;

    SkCanvas *canvas = test->surf->getCanvas();
    canvas->clear(SK_ColorWHITE);

    SkPaint paint;
    paint.setColor(SK_ColorRED);
    paint.setAntiAlias(true);
    canvas->drawCircle(test->width / 2, test->height / 2, 30, paint);

    sk_dump_surface(sk, test->surf, "rt.png");
}

int
main(int argc, const char **argv)
{
    struct canvas_raster_test test = {
        .width = 300,
        .height = 300,
    };

    canvas_raster_test_init(&test);
    canvas_raster_test_draw(&test);
    canvas_raster_test_cleanup(&test);

    return 0;
}
