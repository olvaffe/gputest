/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "spvutil.h"
#include "vkutil.h"

struct compile_test {
    const char *filename;

    struct u_spv spv;
    struct vk vk;
};

static void
compile_test_init(struct compile_test *test)
{
    struct u_spv *spv = &test->spv;
    struct vk *vk = &test->vk;

    spv_init(spv, NULL);
    vk_init(vk, NULL);
}

static void
compile_test_cleanup(struct compile_test *test)
{
    struct u_spv *spv = &test->spv;
    struct vk *vk = &test->vk;

    vk_cleanup(vk);
    spv_cleanup(spv);
}

static void
compile_test_compile(struct compile_test *test)
{
    struct u_spv *spv = &test->spv;

    size_t size;
    void *spirv = spv_compile_file(spv, test->filename, &size);

    spv_log("dumping spirv:");
    spv_dump(spv, spirv, size);

    free(spirv);
}

int
main(int argc, const char **argv)
{
    struct compile_test test = {
        .filename = NULL,
    };

    if (argc != 2)
        vk_die("usage: %s <filename>", argv[0]);
    test.filename = argv[1];

    compile_test_init(&test);
    compile_test_compile(&test);
    compile_test_cleanup(&test);

    return 0;
}
