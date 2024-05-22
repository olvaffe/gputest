/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "spvutil.h"

#include <limits.h>
#include <sstream>
#include <string>

#ifdef HAVE_SPIRV_TOOLS
#include <spirv-tools/libspirv.h>
#endif

#ifdef HAVE_GLSLANG
#include <glslang/Public/resource_limits_c.h>
#endif

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

void
spv_init(struct spv *spv, const struct spv_init_params *params)
{
    memset(spv, 0, sizeof(*spv));
    if (params)
        spv->params = *params;

#ifdef HAVE_GLSLANG
    glslang_initialize_process();
    glslang_resource_t *res = glslang_resource();
    *res = *glslang_default_resource();
#endif
}

void
spv_cleanup(struct spv *spv)
{
#ifdef HAVE_GLSLANG
    glslang_finalize_process();
#endif
}

static bool
spv_init_program_binary(struct spv *spv,
                        struct spv_program *prog,
                        const char *filename,
                        const void *file_data,
                        size_t file_size)
{
    if (file_size < 4 || ((const uint32_t *)file_data)[0] != SpvMagicNumber)
        return false;

    prog->spirv = malloc(file_size);
    if (!prog->spirv)
        spv_die("failed to alloc spirv");

    memcpy(prog->spirv, file_data, file_size);
    prog->size = file_size;

    return true;
}

static bool
spv_init_program_assembly(struct spv *spv,
                          struct spv_program *prog,
                          const char *filename,
                          const void *file_data,
                          size_t file_size)
{
    const char *suffix = strrchr(filename, '.');
    if (!suffix || strcmp(suffix, ".spvasm"))
        return false;

#ifdef HAVE_SPIRV_TOOLS
    const spv_target_env target_env = SPV_ENV_VULKAN_1_2;
    const uint32_t options = SPV_TEXT_TO_BINARY_OPTION_NONE;

    spv_context ctx = spvContextCreate(target_env);

    spv_binary bin;
    spv_diagnostic diag;
    spv_result_t res =
        spvTextToBinaryWithOptions(ctx, (const char *)file_data, file_size, options, &bin, &diag);
    if (res != SPV_SUCCESS)
        spv_die("failed to assemble prog");

    if (!spv_init_program_binary(spv, prog, filename, bin->code,
                                 bin->wordCount * sizeof(uint32_t)))
        spv_die("invalid spirv");

    spvBinaryDestroy(bin);
    spvContextDestroy(ctx);
#else
    spv_die("no spvasm support");
#endif

    return true;
}

#ifdef HAVE_GLSLANG

static glslang_shader_t *
spv_create_glslang_shader(struct spv *spv,
                          glslang_stage_t stage,
                          const void *file_data,
                          size_t file_size)
{
    const glslang_target_client_version_t client_version = GLSLANG_TARGET_VULKAN_1_2;
    const glslang_target_language_version_t target_ver = GLSLANG_TARGET_SPV_1_5;
    const glslang_messages_t messages =
        (glslang_messages_t)(GLSLANG_MSG_DEFAULT_BIT | GLSLANG_MSG_SPV_RULES_BIT |
                             GLSLANG_MSG_VULKAN_RULES_BIT);

    char *glsl = (char *)malloc(file_size + 1);
    if (!glsl)
        spv_die("failed to alloc glsl");
    memcpy(glsl, file_data, file_size);
    glsl[file_size] = '\0';

    const glslang_input_t input = {
        .language = GLSLANG_SOURCE_GLSL,
        .stage = stage,
        .client = GLSLANG_CLIENT_VULKAN,
        .client_version = client_version,
        .target_language = GLSLANG_TARGET_SPV,
        .target_language_version = target_ver,
        .code = glsl,
        .default_version = 100,
        .default_profile = GLSLANG_NO_PROFILE,
        .forward_compatible = true,
        .messages = messages,
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
    const glslang_messages_t messages =
        (glslang_messages_t)(GLSLANG_MSG_DEFAULT_BIT | GLSLANG_MSG_SPV_RULES_BIT |
                             GLSLANG_MSG_VULKAN_RULES_BIT);

    glslang_program_t *prog = glslang_program_create();
    if (!sh)
        spv_die("failed to create program");

    glslang_program_add_shader(prog, sh);

    if (!glslang_program_link(prog, messages))
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

#endif /* HAVE_GLSLANG */

static int
spv_guess_glslang_stage(struct spv *spv, const char *filename)
{
    const char *suffix = strrchr(filename, '.');
    if (!suffix)
        return -1;

    const std::string name(suffix + 1);
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
        return -1;
}

static bool
spv_init_program_glsl(struct spv *spv,
                      struct spv_program *prog,
                      const char *filename,
                      const void *file_data,
                      size_t file_size)
{
    const int stage = spv_guess_glslang_stage(spv, filename);
    if (stage == -1)
        return false;

#ifdef HAVE_GLSLANG
    glslang_shader_t *glsl_sh =
        spv_create_glslang_shader(spv, (glslang_stage_t)stage, file_data, file_size);
    glslang_program_t *glsl_prog = spv_create_glslang_program(spv, glsl_sh);

    prog->spirv =
        spv_transpile_glslang_program(spv, glsl_prog, (glslang_stage_t)stage, &prog->size);

    glslang_program_delete(glsl_prog);
    glslang_shader_delete(glsl_sh);
#else
    spv_die("no glsl support");
#endif

    return true;
}

#if defined(HAVE_CLSPV)

static void *
spv_create_clspv_spirv(struct spv *spv, const void *file_data, size_t file_size, size_t *out_size)
{
    const std::string spv_ver = "1.5";

    std::string std_opts = "-cl-std=CL3.0";
    std_opts += " -cl-single-precision-constant";
    std_opts += " -cl-kernel-arg-info";

    std::string clspv_opts = "-arch=spir";
    clspv_opts += " -spv-version=" + spv_ver;
    clspv_opts += " -inline-entry-points";
    clspv_opts += " -rounding-mode-rte=16,32,64";
    clspv_opts += " -rewrite-packed-structs";
    clspv_opts += " -std430-ubo-layout";
    clspv_opts += " -decorate-nonuniform";
    clspv_opts += " -hack-convert-to-float";
    clspv_opts += " -max-pushconstant-size=128";
    clspv_opts += " -max-ubo-size=16384";
    clspv_opts += " -global-offset";
    clspv_opts += " -long-vector";
    clspv_opts += " -module-constants-in-storage-buffer";

    const std::string opts = std_opts + " " + clspv_opts;

    char *info_log = NULL;
    char *spirv;
    size_t size;
    if (clspvCompileFromSourcesString(1, &file_size, (const char **)&file_data, opts.c_str(),
                                      &spirv, &size, &info_log))
        spv_die("failed to compile kernel:\n%s", info_log);
    free(info_log);

    *out_size = size;
    return spirv;
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
    /* llvm-spirv lacks 1.5 and 1.6 support until v19 */
    const SPIRV::VersionNumber ver = SPIRV::VersionNumber::SPIRV_1_4;

    const SPIRV::TranslatorOpts opts(ver);
    std::ostringstream spirv_ss;
    std::string info_log;
    if (!llvm::writeSpirv(mod, opts, spirv_ss, info_log))
        spv_die("failed to translate to spirv: %s", info_log.c_str());

    const std::string spirv_str = spirv_ss.str();
    const size_t size = spirv_str.size();

    void *spirv = malloc(size);
    if (!spirv)
        spv_die("failed to alloc spirv");
    memcpy(spirv, spirv_str.data(), size);

    *out_size = size;
    return spirv;
}

#endif /* HAVE_CLSPV */

static bool
spv_init_program_clc(struct spv *spv,
                     struct spv_program *prog,
                     const char *filename,
                     const void *file_data,
                     size_t file_size)
{
    const char *suffix = strrchr(filename, '.');
    if (!suffix || strcmp(suffix, ".cl"))
        return false;

#if defined(HAVE_CLSPV)
    prog->spirv = spv_create_clspv_spirv(spv, file_data, file_size, &prog->size);
#elif defined(HAVE_LLVM_SPIRV)
    llvm::LLVMContext ctx;
    std::unique_ptr<llvm::Module> mod = spv_create_llvm_module_from_kernel(spv, &ctx, filename);
    prog->spirv = spv_create_spirv_from_llvm_module(spv, mod.get(), &prog->size);
#else
    spv_die("no opencl c support");
#endif

    return true;
}

static void
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

    prog->reflection.execution_model = mod.spirv_execution_model;
    prog->reflection.entrypoint = strdup(mod.entry_point_name);
    prog->reflection.set_count = set_count;
    prog->reflection.sets = sets;

    spvReflectDestroyShaderModule(&mod);
#endif /* HAVE_SPIRV_REFLECT */
}

struct spv_program *
spv_create_program(struct spv *spv, const char *filename)
{
    struct spv_program *prog = (struct spv_program *)calloc(1, sizeof(*prog));
    if (!prog)
        spv_die("failed to alloc prog");

    size_t file_size;
    const void *file_data = u_map_file(filename, &file_size);

    if (!spv_init_program_binary(spv, prog, filename, file_data, file_size) &&
        !spv_init_program_assembly(spv, prog, filename, file_data, file_size) &&
        !spv_init_program_glsl(spv, prog, filename, file_data, file_size) &&
        !spv_init_program_clc(spv, prog, filename, file_data, file_size))
        spv_die("failed to load %s", filename);

    u_unmap_file(file_data, file_size);

    spv_reflect_program(spv, prog);

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
spv_disasm_program(struct spv *spv, struct spv_program *prog)
{
#ifdef HAVE_SPIRV_TOOLS
    const spv_target_env target_env = SPV_ENV_VULKAN_1_2;

    uint32_t options =
        SPV_BINARY_TO_TEXT_OPTION_INDENT | SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES;
    if (u_isatty())
        options |= SPV_BINARY_TO_TEXT_OPTION_COLOR;

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
#endif
}
