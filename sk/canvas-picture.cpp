/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "include/core/SkPicture.h"
#include "include/core/SkPictureRecorder.h"
#include "skutil.h"

struct canvas_picture_test {
    uint32_t width;
    uint32_t height;

    struct sk sk;
    sk_sp<SkSurface> surf;
    sk_sp<SkPicture> pic;
};

static void
canvas_picture_test_init_picture(struct canvas_picture_test *test)
{
    SkPictureRecorder rec;
    SkCanvas *canvas =
        rec.beginRecording(SkIntToScalar(test->width), SkIntToScalar(test->height));
    canvas->clear(SK_ColorWHITE);

    SkPaint paint;
    paint.setColor(SK_ColorRED);
    paint.setAntiAlias(true);
    canvas->drawCircle(test->width / 2, test->height / 2, 30, paint);

    test->pic = rec.finishRecordingAsPicture();
}

static void
canvas_picture_test_init(struct canvas_picture_test *test)
{
    struct sk *sk = &test->sk;

    sk_init(sk, NULL);
    test->surf = sk_create_surface_raster(sk, test->width, test->height);
    canvas_picture_test_init_picture(test);
}

static void
canvas_picture_test_cleanup(struct canvas_picture_test *test)
{
    struct sk *sk = &test->sk;

    test->pic.reset();
    test->surf.reset();
    sk_cleanup(sk);
}

static void
canvas_picture_test_draw(struct canvas_picture_test *test)
{
    struct sk *sk = &test->sk;

    SkCanvas *canvas = test->surf->getCanvas();
    test->pic->playback(canvas);

    sk_dump_surface(sk, test->surf, "rt.png");
}

int
main(int argc, const char **argv)
{
    struct canvas_picture_test test = {
        .width = 300,
        .height = 300,
    };

    canvas_picture_test_init(&test);
    canvas_picture_test_draw(&test);
    canvas_picture_test_cleanup(&test);

    return 0;
}
