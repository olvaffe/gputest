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

static VkPipelineLayout
compile_test_create_pipeline_layout(struct compile_test *test, struct spv_program *prog)
{
    const VkShaderStageFlags stage = VK_SHADER_STAGE_COMPUTE_BIT;
    struct vk *vk = &test->vk;

    VkDescriptorSetLayout *set_layouts =
        malloc(sizeof(*set_layouts) * prog->reflection.set_count);
    if (!set_layouts)
        vk_die("failed to alloc set layouts");

    for (uint32_t i = 0; i < prog->reflection.set_count; i++) {
        const struct spv_program_reflection_set *set = &prog->reflection.sets[i];

        VkDescriptorSetLayoutBinding *bindings = malloc(sizeof(*bindings) * set->binding_count);
        if (!bindings)
            vk_die("failed to alloc bindings");

        for (uint32_t j = 0; j < set->binding_count; j++) {
            const struct spv_program_reflection_binding *binding = &set->bindings[j];

            VkDescriptorType type;
            switch (binding->type) {
            case 4: /* SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER */
                type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                break;
            case 5: /* SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER */
                type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
                break;
            case 6: /* SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER */
                type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                break;
            case 7: /* SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER */
                type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                break;
            default:
                vk_die("bad type");
                break;
            }

            bindings[j] = (VkDescriptorSetLayoutBinding){
                .binding = binding->binding,
                .descriptorType = type,
                .descriptorCount = binding->count,
                .stageFlags = stage,
            };
        }

        const VkDescriptorSetLayoutCreateInfo set_layout_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = set->binding_count,
            .pBindings = bindings,
        };
        vk->result =
            vk->CreateDescriptorSetLayout(vk->dev, &set_layout_info, NULL, &set_layouts[i]);
        vk_check(vk, "failed to create set layout");

        free(bindings);
    }

    const VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = prog->reflection.set_count,
        .pSetLayouts = set_layouts,
    };
    VkPipelineLayout pipeline_layout;
    vk->result = vk->CreatePipelineLayout(vk->dev, &pipeline_layout_info, NULL, &pipeline_layout);
    vk_check(vk, "failed to create pipeline layout");

    for (uint32_t i = 0; i < prog->reflection.set_count; i++)
        vk->DestroyDescriptorSetLayout(vk->dev, set_layouts[i], NULL);
    free(set_layouts);

    return pipeline_layout;
}

static void
compile_test_compile_compute_pipeline(struct compile_test *test, struct spv_program *prog)
{
    const VkShaderStageFlags stage = VK_SHADER_STAGE_COMPUTE_BIT;
    struct vk *vk = &test->vk;
    struct spv *spv = &test->spv;

    spv_reflect_program(spv, prog);

    const VkComputePipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = stage,
            .module = vk_create_shader_module(vk, prog->spirv, prog->size),
            .pName = prog->reflection.entrypoint,
        },
        .layout = compile_test_create_pipeline_layout(test, prog),
    };
    VkPipeline pipeline;
    vk->result =
        vk->CreateComputePipelines(vk->dev, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline);
    vk_check(vk, "failed to create pipeline");

    vk->DestroyShaderModule(vk->dev, pipeline_info.stage.module, NULL);
    vk->DestroyPipelineLayout(vk->dev, pipeline_info.layout, NULL);

    vk->DestroyPipeline(vk->dev, pipeline, NULL);
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

    if (test->disasm)
        spv_disasm_program(spv, prog);

    if (prog->stage == GLSLANG_STAGE_COMPUTE)
        compile_test_compile_compute_pipeline(test, prog);

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
