/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "spvutil.h"
#include "vkutil.h"

struct compile_test {
    const char *filename;
    bool disasm;

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
    struct vk *vk = &test->vk;

    struct spv_program *prog;
    if (spv_guess_shader(spv, test->filename)) {
        const glslang_stage_t stage = spv_guess_stage(spv, test->filename);
        prog = spv_create_program_from_shader(spv, stage, test->filename);
    } else {
        prog = spv_create_program_from_kernel(spv, test->filename);
    }

    if (test->disasm)
        spv_disasm_program(spv, prog);

    if (prog->stage == GLSLANG_STAGE_COMPUTE) {
        const VkShaderStageFlags stages = VK_SHADER_STAGE_COMPUTE_BIT;

        spv_reflect_program(spv, prog);
        const struct spv_program_reflection *reflection = &prog->reflection;

        struct vk_pipeline *pipeline = vk_create_pipeline(vk);
        vk_add_pipeline_shader(vk, pipeline, stages, prog->spirv, prog->size);

        for (uint32_t i = 0; i < reflection->set_count; i++) {
            const struct spv_program_reflection_set *set = &reflection->sets[i];
            const struct spv_program_reflection_binding *binding = &set->bindings[0];
            if (set->binding_count > 1 || binding->binding)
                vk_die("bad bindings");

            VkDescriptorType type;
            switch (binding->storage) {
            case 5: /* EvqUniform */
                type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                break;
            case 6: /* EvqBuffer */
                type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                break;
            default:
                vk_die("bad storage");
                break;
            }

            vk_add_pipeline_set_layout(vk, pipeline, type, binding->count, stages, NULL);
        }

        vk_setup_pipeline(vk, pipeline, NULL);
        vk_compile_pipeline(vk, pipeline);
        vk_destroy_pipeline(vk, pipeline);
    }

    spv_destroy_program(spv, prog);
}

int
main(int argc, const char **argv)
{
    struct compile_test test = {
        .filename = NULL,
        .disasm = true,
    };

    if (argc != 2)
        vk_die("usage: %s <filename>", argv[0]);
    test.filename = argv[1];

    compile_test_init(&test);
    compile_test_compile(&test);
    compile_test_cleanup(&test);

    return 0;
}
