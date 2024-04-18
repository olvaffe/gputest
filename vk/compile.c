/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "spvutil.h"
#include "vkutil.h"

struct compile_test {
    const char *filename;

    struct spv spv;
    struct vk vk;
};

static void
compile_test_init(struct compile_test *test)
{
    struct spv *spv = &test->spv;
    struct vk *vk = &test->vk;

    spv_init(spv, NULL);
    vk_init(vk, NULL);
}

static void
compile_test_cleanup(struct compile_test *test)
{
    struct spv *spv = &test->spv;
    struct vk *vk = &test->vk;

    vk_cleanup(vk);
    spv_cleanup(spv);
}

static void
compile_test_compile(struct compile_test *test)
{
    struct spv *spv = &test->spv;

    struct spv_program *prog;
    if (spv_guess_shader(spv, test->filename)) {
        const glslang_stage_t stage = spv_guess_stage(spv, test->filename);
        prog = spv_create_program_from_shader(spv, stage, test->filename);
    } else {
        prog = spv_create_program_from_kernel(spv, test->filename);
    }

    spv_disasm_program(spv, prog);

    spv_destroy_program(spv, prog);
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
