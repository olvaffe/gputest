/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "include/utils/SkNullCanvas.h"
#include "skutil.h"

struct canvas_null_test {
    struct sk sk;
    std::unique_ptr<SkCanvas> canvas;
};

static void
canvas_null_test_init(struct canvas_null_test *test)
{
    struct sk *sk = &test->sk;

    sk_init(sk, NULL);
    test->canvas = SkMakeNullCanvas();
}

static void
canvas_null_test_cleanup(struct canvas_null_test *test)
{
    struct sk *sk = &test->sk;

    test->canvas.reset();
    sk_cleanup(sk);
}

static void
canvas_null_test_draw(struct canvas_null_test *test)
{
    SkCanvas *canvas = test->canvas.get();
    canvas->clear(SK_ColorWHITE);
}

int
main(int argc, const char **argv)
{
    struct canvas_null_test test = {};

    canvas_null_test_init(&test);
    canvas_null_test_draw(&test);
    canvas_null_test_cleanup(&test);

    return 0;
}
