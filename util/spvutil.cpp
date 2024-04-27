/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "spvutil.h"

#include <glslang/Public/resource_limits_c.h>
#include <limits.h>
#include <spirv-tools/libspirv.h>
#include <sstream>
#include <string>

#if defined(HAVE_CLSPV)
#include <clspv/Compiler.h>
#elif defined(HAVE_LLVM_SPIRV)
#include <LLVMSPIRVLib/LLVMSPIRVLib.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Config/config.h>
#include <clang/Driver/Driver.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <dlfcn.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>
#endif /* HAVE_CLSPV */

#ifdef HAVE_SPIRV_REFLECT
#include <spirv_reflect.h>
#endif

static void
spv_init_params(struct spv *spv, const struct spv_init_params *params)
{
    if (params)
        spv->params = *params;

    if (!spv->params.glsl_client_version)
        spv->params.glsl_client_version = GLSLANG_TARGET_VULKAN_1_2;

    spv->params.glsl_messages =
        (glslang_messages_t)((int)spv->params.glsl_messages |
                             (GLSLANG_MSG_DEFAULT_BIT | GLSLANG_MSG_SPV_RULES_BIT |
                              GLSLANG_MSG_VULKAN_RULES_BIT));
}

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
    spv_init_params(spv, params);
    spv_init_glslang(spv);
}

void
spv_cleanup(struct spv *spv)
{
    glslang_finalize_process();
}

int
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
    else if (name == "cl")
        return SPV_STAGE_KERNEL;
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

    glslang_target_language_version_t target_ver;
    switch (spv->params.glsl_client_version) {
    case GLSLANG_TARGET_VULKAN_1_0:
    default:
        target_ver = GLSLANG_TARGET_SPV_1_0;
        break;
    case GLSLANG_TARGET_VULKAN_1_1:
        target_ver = GLSLANG_TARGET_SPV_1_3;
        break;
    case GLSLANG_TARGET_VULKAN_1_2:
        target_ver = GLSLANG_TARGET_SPV_1_5;
        break;
    case GLSLANG_TARGET_VULKAN_1_3:
        target_ver = GLSLANG_TARGET_SPV_1_6;
        break;
    }

    const glslang_input_t input = {
        .language = GLSLANG_SOURCE_GLSL,
        .stage = stage,
        .client = GLSLANG_CLIENT_VULKAN,
        .client_version = spv->params.glsl_client_version,
        .target_language = GLSLANG_TARGET_SPV,
        .target_language_version = target_ver,
        .code = glsl,
        .default_version = 100,
        .default_profile = GLSLANG_NO_PROFILE,
        .forward_compatible = true,
        .messages = spv->params.glsl_messages,
        .resource = glslang_resource(),
    };

    glslang_shader_t *sh = glslang_shader_create(&input);
    if (!sh)
        spv_die("failed to create shader");
    if (!glslang_shader_preprocess(sh, &input) || !glslang_shader_parse(sh, &input))
        spv_die("failed to parse shader:\n%s", glslang_shader_get_info_log(sh));

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

    if (!glslang_program_link(prog, spv->params.glsl_messages))
        spv_die("failed to link program:\n%s", glslang_program_get_info_log(prog));

    glslang_program_map_io(prog);

    return prog;
}

static void *
spv_transpile_glslang_program(struct spv *spv,
                              glslang_program_t *prog,
                              glslang_stage_t stage,
                              size_t *out_size)
{
    glslang_program_SPIRV_generate(prog, stage);
    const char *info_log = glslang_program_SPIRV_get_messages(prog);
    if (info_log)
        spv_die("failed to transpile program:\n%s", info_log);

    const size_t size = glslang_program_SPIRV_get_size(prog) * 4;
    void *spirv = malloc(size);
    if (!spirv)
        spv_die("failed to alloc spirv");
    memcpy(spirv, glslang_program_SPIRV_get_ptr(prog), size);

    *out_size = size;
    return spirv;
}

#if defined(HAVE_CLSPV)

static void *
spv_create_clspv_spirv(struct spv *spv, const char *filename, size_t *out_size)
{
    std::string opts = "-cl-std=CL3.0 -inline-entry-points";
    opts += " -cl-single-precision-constant";
    opts += " -cl-kernel-arg-info";
    opts += " -rounding-mode-rte=16,32,64";
    opts += " -rewrite-packed-structs";
    opts += " -std430-ubo-layout";
    opts += " -decorate-nonuniform";
    opts += " -hack-convert-to-float";
    opts += " -arch=spir";
    opts += " -spv-version=1.5";
    opts += " -max-pushconstant-size=128";
    opts += " -max-ubo-size=16384";
    opts += " -global-offset";
    opts += " -long-vector";
    opts += " -module-constants-in-storage-buffer";
    opts += " -cl-arm-non-uniform-work-group-size";

    size_t file_size;
    const void *file_data = u_map_file(filename, &file_size);

    char *info_log = NULL;
    char *spirv;
    size_t size;
    if (clspvCompileFromSourcesString(1, &file_size, (const char **)&file_data, opts.c_str(),
                                      &spirv, &size, &info_log))
        spv_die("failed to compile kernel:\n%s", info_log);
    free(info_log);

    u_unmap_file(file_data, file_size);

    *out_size = size;
    return spirv
}

#elif defined(HAVE_LLVM_SPIRV)

static std::unique_ptr<llvm::Module>
spv_create_llvm_module_from_kernel(struct spv *spv, llvm::LLVMContext *ctx, const char *filename)
{
    clang::CompilerInstance c;

    const std::vector<const char *> opts = {
        "-triple=spir64-unknown-unknown",
        "-cl-std=CL3.0",
        "-O2",
        filename,
    };

    std::string diag_log;
    llvm::raw_string_ostream diag_stream{ diag_log };
    clang::DiagnosticsEngine diag_engine{ new clang::DiagnosticIDs, new clang::DiagnosticOptions,
                                          new clang::TextDiagnosticPrinter{
                                              diag_stream, &c.getDiagnosticOpts() } };

    if (!clang::CompilerInvocation::CreateFromArgs(c.getInvocation(), opts, diag_engine) ||
        diag_engine.hasErrorOccurred())
        spv_die("failed to create invocation: %s", diag_log.c_str());

    c.getDiagnosticOpts().ShowCarets = false;
    c.createDiagnostics(new clang::TextDiagnosticPrinter{ diag_stream, &c.getDiagnosticOpts() });

    c.getFrontendOpts().ProgramAction = clang::frontend::EmitLLVMOnly;

    {
        Dl_info lib_info;
        if (!dladdr((const void *)clang::CompilerInvocation::CreateFromArgs, &lib_info))
            spv_die("failed to query libclang-cpp: %s", dlerror());

        char *lib_path = realpath(lib_info.dli_fname, NULL);
        if (!lib_path)
            spv_die("failed to get real path of %s", lib_info.dli_fname);
        std::string res_path =
            clang::driver::Driver::GetResourcesPath(lib_path, CLANG_RESOURCE_DIR);
        free(lib_path);

        c.getHeaderSearchOpts().AddPath(res_path + "/include", clang::frontend::Angled, false,
                                        false);
    }

    c.getPreprocessorOpts().Includes.push_back("opencl-c.h");

    clang::EmitLLVMOnlyAction act(ctx);
    if (!c.ExecuteAction(act))
        spv_die("failed to translate CLC:\n%s", diag_log.c_str());

    return act.takeModule();
}

static void *
spv_create_spirv_from_llvm_module(struct spv *spv, llvm::Module *mod, size_t *out_size)
{
    std::ostringstream spv_stream;
    std::string log;
    SPIRV::TranslatorOpts spirv_opts;
    if (!llvm::writeSpirv(mod, spirv_opts, spv_stream, log))
        spv_die("failed to translate to spirv: %s", log.c_str());

    std::string spirv = spv_stream.str();
    void *data = malloc(spirv.size());
    memcpy(data, spirv.data(), spirv.size());
    *out_size = spirv.size();

    return data;
}

#endif /* HAVE_CLSPV */

struct spv_program *
spv_create_program(struct spv *spv, int stage, const char *filename)
{
    struct spv_program *prog = (struct spv_program *)calloc(1, sizeof(*prog));
    if (!prog)
        spv_die("failed to alloc prog");

    prog->stage = stage;

    if (stage == SPV_STAGE_KERNEL) {
#if defined(HAVE_CLSPV)
        prog->spirv = spv_create_clspv_spirv(spv, filename, &prog->size);
#elif defined(HAVE_LLVM_SPIRV)
        llvm::LLVMContext ctx;
        std::unique_ptr<llvm::Module> mod =
            spv_create_llvm_module_from_kernel(spv, &ctx, filename);
        prog->spirv = spv_create_spirv_from_llvm_module(spv, mod.get(), &prog->size);
#else
        spv_die("no opencl c support");
#endif
    } else {
        glslang_shader_t *glsl_sh =
            spv_create_glslang_shader(spv, (glslang_stage_t)stage, filename);
        glslang_program_t *glsl_prog = spv_create_glslang_program(spv, glsl_sh);

        prog->spirv =
            spv_transpile_glslang_program(spv, glsl_prog, (glslang_stage_t)stage, &prog->size);

        glslang_program_delete(glsl_prog);
        glslang_shader_delete(glsl_sh);
    }

    return prog;
}

void
spv_destroy_program(struct spv *spv, struct spv_program *prog)
{
    struct spv_program_reflection *reflection = &prog->reflection;

    free(reflection->entrypoint);
    for (uint32_t i = 0; i < prog->reflection.set_count; i++) {
        struct spv_program_reflection_set *set = &reflection->sets[i];
        free(set->bindings);
    }
    free(reflection->sets);

    free(prog->spirv);
    free(prog);
}

void
spv_reflect_program(struct spv *spv, struct spv_program *prog)
{
#ifdef HAVE_SPIRV_REFLECT
    SpvReflectShaderModule mod;
    SpvReflectResult res = spvReflectCreateShaderModule(prog->size, prog->spirv, &mod);
    if (res != SPV_REFLECT_RESULT_SUCCESS)
        spv_die("failed to reflect spirv");

    uint32_t max_set = 0;
    for (uint32_t i = 0; i < mod.descriptor_set_count; i++) {
        const SpvReflectDescriptorSet *set = &mod.descriptor_sets[i];
        if (max_set < set->set)
            max_set = set->set;
    }

    const uint32_t set_count = max_set + 1;
    struct spv_program_reflection_set *sets =
        (struct spv_program_reflection_set *)calloc(set_count, sizeof(*sets));
    if (!sets)
        spv_die("failed to alloc sets");

    for (uint32_t i = 0; i < mod.descriptor_set_count; i++) {
        const SpvReflectDescriptorSet *src = &mod.descriptor_sets[i];
        struct spv_program_reflection_set *dst = &sets[src->set];

        dst->binding_count = src->binding_count;
        dst->bindings = (struct spv_program_reflection_binding *)calloc(src->binding_count,
                                                                        sizeof(*dst->bindings));
        if (!dst->bindings)
            spv_die("failed to alloc bindings");

        for (uint32_t j = 0; j < src->binding_count; j++) {
            const SpvReflectDescriptorBinding *s = src->bindings[j];
            struct spv_program_reflection_binding *d = &dst->bindings[j];
            d->binding = s->binding;
            d->type = s->descriptor_type;
            d->count = s->count;
        }
    }

    prog->reflection.entrypoint = strdup(mod.entry_point_name);
    prog->reflection.set_count = set_count;
    prog->reflection.sets = sets;

    spvReflectDestroyShaderModule(&mod);
#endif /* HAVE_SPIRV_REFLECT */
}

void
spv_disasm_program(struct spv *spv, struct spv_program *prog)
{
    const uint32_t options = SPV_BINARY_TO_TEXT_OPTION_COLOR | SPV_BINARY_TO_TEXT_OPTION_INDENT |
                             SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES;

    spv_target_env target_env;
    switch (spv->params.glsl_client_version) {
    case GLSLANG_TARGET_VULKAN_1_0:
    default:
        target_env = SPV_ENV_VULKAN_1_0;
        break;
    case GLSLANG_TARGET_VULKAN_1_1:
        target_env = SPV_ENV_VULKAN_1_1;
        break;
    case GLSLANG_TARGET_VULKAN_1_2:
        target_env = SPV_ENV_VULKAN_1_2;
        break;
    case GLSLANG_TARGET_VULKAN_1_3:
        target_env = SPV_ENV_VULKAN_1_3;
        break;
    }

    spv_context ctx = spvContextCreate(target_env);

    spv_text txt;
    spv_diagnostic diag;
    spv_result_t res =
        spvBinaryToText(ctx, (const uint32_t *)prog->spirv, prog->size / 4, options, &txt, &diag);
    if (res != SPV_SUCCESS)
        spv_die("failed to disasm prog");

    spv_log("spirv disassembly:\n%s", txt->str);

    spvTextDestroy(txt);
    spvContextDestroy(ctx);
}
