/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "include/docs/SkPDFDocument.h"
#include "skutil.h"

struct canvas_pdf_test {
    uint32_t width;
    uint32_t height;

    struct sk sk;
    std::unique_ptr<SkFILEWStream> writer;
    sk_sp<SkDocument> doc;
};

static void
canvas_pdf_test_init_doc(struct canvas_pdf_test *test)
{
    test->writer = std::make_unique<SkFILEWStream>("rt.pdf");
    if (!test->writer->isValid())
        sk_die("failed to open file");

    const SkPDF::Metadata metadata;
    test->doc = SkPDF::MakeDocument(test->writer.get(), metadata);
}

static void
canvas_pdf_test_init(struct canvas_pdf_test *test)
{
    struct sk *sk = &test->sk;

    sk_init(sk, NULL);
    canvas_pdf_test_init_doc(test);
}

static void
canvas_pdf_test_cleanup(struct canvas_pdf_test *test)
{
    struct sk *sk = &test->sk;

    test->doc.reset();
    test->writer.reset();
    sk_cleanup(sk);
}

static void
canvas_pdf_test_draw(struct canvas_pdf_test *test)
{
    SkCanvas *canvas =
        test->doc->beginPage(SkIntToScalar(test->width), SkIntToScalar(test->height));
    canvas->clear(SK_ColorWHITE);

    SkPaint paint;
    paint.setColor(SK_ColorRED);
    paint.setAntiAlias(true);
    canvas->drawCircle(test->width / 2, test->height / 2, 30, paint);

    test->doc->endPage();
    test->doc->close();
}

int
main(int argc, const char **argv)
{
    struct canvas_pdf_test test = {
        .width = 300,
        .height = 300,
    };

    canvas_pdf_test_init(&test);
    canvas_pdf_test_draw(&test);
    canvas_pdf_test_cleanup(&test);

    return 0;
}
