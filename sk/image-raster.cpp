/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "skutil.h"

struct image_raster_test {
    const char *filename;

    struct sk sk;
    sk_sp<SkImage> img;
    sk_sp<SkSurface> surf;
};

static void
image_raster_test_init(struct image_raster_test *test)
{
    struct sk *sk = &test->sk;

    sk_init(sk, NULL);
    test->img = sk_load_png(sk, test->filename);
    test->surf = sk_create_surface_raster(sk, test->img->width(), test->img->height());
}

static void
image_raster_test_cleanup(struct image_raster_test *test)
{
    struct sk *sk = &test->sk;

    test->surf.reset();
    test->img.reset();
    sk_cleanup(sk);
}

static void
image_raster_test_draw(struct image_raster_test *test)
{
    struct sk *sk = &test->sk;

    SkCanvas *canvas = test->surf->getCanvas();
    canvas->clear(SK_ColorWHITE);

    canvas->drawImage(test->img, 0, 0);

    sk_dump_surface(sk, test->surf, "rt.png");
}

int
main(int argc, const char **argv)
{
    struct image_raster_test test = {
        .filename = NULL,
    };

    if (argc != 2)
        sk_die("usage: %s <png-file>", argv[0]);

    test.filename = argv[1];

    image_raster_test_init(&test);
    image_raster_test_draw(&test);
    image_raster_test_cleanup(&test);

    return 0;
}
