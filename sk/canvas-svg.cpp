/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "include/svg/SkSVGCanvas.h"
#include "skutil.h"

struct canvas_svg_test {
    uint32_t width;
    uint32_t height;

    struct sk sk;
    std::unique_ptr<SkFILEWStream> writer;
    std::unique_ptr<SkCanvas> canvas;
};

static void
canvas_svg_test_init_canvas(struct canvas_svg_test *test)
{
    test->writer = std::make_unique<SkFILEWStream>("rt.svg");
    if (!test->writer->isValid())
        sk_die("failed to open file");

    const SkRect bounds = SkRect::MakeIWH(test->width, test->height);
    test->canvas = SkSVGCanvas::Make(bounds, test->writer.get());
}

static void
canvas_svg_test_init(struct canvas_svg_test *test)
{
    struct sk *sk = &test->sk;

    sk_init(sk, NULL);
    canvas_svg_test_init_canvas(test);
}

static void
canvas_svg_test_cleanup(struct canvas_svg_test *test)
{
    struct sk *sk = &test->sk;

    test->canvas.reset();
    test->writer.reset();
    sk_cleanup(sk);
}

static void
canvas_svg_test_draw(struct canvas_svg_test *test)
{
    SkCanvas *canvas = test->canvas.get();
    canvas->clear(SK_ColorWHITE);

    SkPaint paint;
    paint.setColor(SK_ColorRED);
    paint.setAntiAlias(true);
    canvas->drawCircle(test->width / 2, test->height / 2, 30, paint);
}

int
main(int argc, const char **argv)
{
    struct canvas_svg_test test = {
        .width = 300,
        .height = 300,
    };

    canvas_svg_test_init(&test);
    canvas_svg_test_draw(&test);
    canvas_svg_test_cleanup(&test);

    return 0;
}
