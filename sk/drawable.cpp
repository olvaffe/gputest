/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "include/core/SkDrawable.h"
#include "skutil.h"

struct drawable_test {
    uint32_t width;
    uint32_t height;

    struct sk sk;
    sk_sp<SkSurface> surf;
    std::unique_ptr<SkDrawable> drawable;
};

class drawable_test_drawable : public SkDrawable {
  public:
    drawable_test_drawable(struct drawable_test *test) : test_(test) {}

    SkRect onGetBounds() override { return SkRect::MakeIWH(test_->width, test_->height); }

    void onDraw(SkCanvas *canvas) override
    {
        canvas->clear(SK_ColorWHITE);

        SkPaint paint;
        paint.setColor(SK_ColorRED);
        paint.setAntiAlias(true);
        canvas->drawCircle(test_->width / 2, test_->height / 2, 30, paint);
    }

  private:
    struct drawable_test *test_;
};

static void
drawable_test_init(struct drawable_test *test)
{
    struct sk *sk = &test->sk;

    sk_init(sk, NULL);
    test->surf = sk_create_surface_raster(sk, test->width, test->height);
    test->drawable = std::make_unique<drawable_test_drawable>(test);
}

static void
drawable_test_cleanup(struct drawable_test *test)
{
    struct sk *sk = &test->sk;

    test->drawable.reset();
    test->surf.reset();
    sk_cleanup(sk);
}

static void
drawable_test_draw(struct drawable_test *test)
{
    struct sk *sk = &test->sk;

    SkCanvas *canvas = test->surf->getCanvas();
    canvas->drawDrawable(test->drawable.get());

    sk_dump_surface(sk, test->surf, "rt.png");
}

int
main(int argc, const char **argv)
{
    struct drawable_test test = {
        .width = 300,
        .height = 300,
    };

    drawable_test_init(&test);
    drawable_test_draw(&test);
    drawable_test_cleanup(&test);

    return 0;
}
