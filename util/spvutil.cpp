/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "spvutil.h"

#include <glslang/Public/resource_limits_c.h>
#include <memory>
#include <spirv-tools/libspirv.h>
#include <sstream>
#include <string>

static inline void
spv_init_glslang(struct spv *spv)
{
    glslang_initialize_process();

    glslang_resource_t *res = glslang_resource();
    *res = *glslang_default_resource();
}

void
spv_init(struct spv *spv, const struct spv_init_params *params)
{
    memset(spv, 0, sizeof(*spv));

    if (params)
        spv->params = *params;

    if (!spv->params.messages) {
        spv->params.messages =
            (glslang_messages_t)(GLSLANG_MSG_DEFAULT_BIT | GLSLANG_MSG_SPV_RULES_BIT |
                                 GLSLANG_MSG_VULKAN_RULES_BIT);
    }

    spv_init_glslang(spv);
}

void
spv_cleanup(struct spv *spv)
{
    glslang_finalize_process();
}

glslang_stage_t
spv_guess_stage(struct spv *spv, const char *filename)
{
    const char *suffix = strrchr(filename, '.');
    if (!suffix)
        spv_die("%s has no suffix", filename);
    suffix++;

    const std::string name(suffix);
    if (name == "vert")
        return GLSLANG_STAGE_VERTEX;
    else if (name == "tesc")
        return GLSLANG_STAGE_TESSCONTROL;
    else if (name == "tese")
        return GLSLANG_STAGE_TESSEVALUATION;
    else if (name == "geom")
        return GLSLANG_STAGE_GEOMETRY;
    else if (name == "frag")
        return GLSLANG_STAGE_FRAGMENT;
    else if (name == "comp")
        return GLSLANG_STAGE_COMPUTE;
    else if (name == "rgen")
        return GLSLANG_STAGE_RAYGEN;
    else if (name == "rint")
        return GLSLANG_STAGE_INTERSECT;
    else if (name == "rahit")
        return GLSLANG_STAGE_ANYHIT;
    else if (name == "rchit")
        return GLSLANG_STAGE_CLOSESTHIT;
    else if (name == "rmiss")
        return GLSLANG_STAGE_MISS;
    else if (name == "rcall")
        return GLSLANG_STAGE_CALLABLE;
    else if (name == "task")
        return GLSLANG_STAGE_TASK;
    else if (name == "mesh")
        return GLSLANG_STAGE_MESH;
    else
        spv_die("bad stage name %s", name.c_str());
}

static glslang_shader_t *
spv_create_glslang_shader(struct spv *spv, glslang_stage_t stage, const char *filename)
{
    size_t file_size;
    const void *file_data = u_map_file(filename, &file_size);

    char *glsl = (char *)malloc(file_size + 1);
    if (!glsl)
        spv_die("failed to alloc glsl");
    memcpy(glsl, file_data, file_size);
    glsl[file_size] = '\0';

    u_unmap_file(file_data, file_size);

    const glslang_input_t input = {
        .language = GLSLANG_SOURCE_GLSL,
        .stage = stage,
        .client = GLSLANG_CLIENT_VULKAN,
        .client_version = GLSLANG_TARGET_VULKAN_1_2,
        .target_language = GLSLANG_TARGET_SPV,
        .target_language_version = GLSLANG_TARGET_SPV_1_2,
        .code = glsl,
        .default_version = 100,
        .default_profile = GLSLANG_NO_PROFILE,
        .forward_compatible = true,
        .messages = spv->params.messages,
        .resource = glslang_resource(),
    };

    glslang_shader_t *sh = glslang_shader_create(&input);
    if (!sh)
        spv_die("failed to create shader");
    if (!glslang_shader_preprocess(sh, &input) || !glslang_shader_parse(sh, &input))
        spv_die("failed to parse shader: %s", glslang_shader_get_info_log(sh));

    free(glsl);

    return sh;
}

static glslang_program_t *
spv_create_glslang_program(struct spv *spv, glslang_shader_t *sh)
{
    glslang_program_t *prog = glslang_program_create();
    if (!sh)
        spv_die("failed to create program");

    glslang_program_add_shader(prog, sh);

    if (!glslang_program_link(prog, spv->params.messages))
        spv_die("failed to link program: %s", glslang_program_get_info_log(prog));

    glslang_program_map_io(prog);

    return prog;
}

static const void *
spv_transpile_glslang_program(struct spv *spv,
                              glslang_program_t *prog,
                              glslang_stage_t stage,
                              size_t *out_size)
{
    glslang_program_SPIRV_generate(prog, stage);
    const char *info_log = glslang_program_SPIRV_get_messages(prog);
    if (info_log)
        spv_die("failed to transpile program: %s", info_log);

    *out_size = glslang_program_SPIRV_get_size(prog) * 4;
    return glslang_program_SPIRV_get_ptr(prog);
}

struct spv_program *
spv_create_program_from_shader(struct spv *spv, glslang_stage_t stage, const char *filename)
{
    struct spv_program *prog = (struct spv_program *)calloc(1, sizeof(*prog));
    if (!prog)
        spv_die("failed to alloc prog");

    prog->glsl_sh = spv_create_glslang_shader(spv, stage, filename);
    prog->glsl_prog = spv_create_glslang_program(spv, prog->glsl_sh);
    prog->spirv = spv_transpile_glslang_program(spv, prog->glsl_prog, stage, &prog->size);

    return prog;
}

void
spv_destroy_program(struct spv *spv, struct spv_program *prog)
{
    glslang_shader_delete(prog->glsl_sh);
    glslang_program_delete(prog->glsl_prog);

    free(prog);
}

void
spv_disasm_program(struct spv *spv, struct spv_program *prog)
{
    spv_context ctx = spvContextCreate(SPV_ENV_VULKAN_1_2);

    spv_text txt;
    spv_diagnostic diag;
    spv_result_t res =
        spvBinaryToText(ctx, (const uint32_t *)prog->spirv, prog->size / 4,
                        SPV_BINARY_TO_TEXT_OPTION_COLOR | SPV_BINARY_TO_TEXT_OPTION_INDENT |
                            SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES,
                        &txt, &diag);
    if (res != SPV_SUCCESS)
        spv_die("failed to disasm prog");

    spv_log("spirv disassembly:\n%s", txt->str);

    spvTextDestroy(txt);
    spvContextDestroy(ctx);
}
