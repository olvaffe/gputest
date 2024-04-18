/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "spvutil.h"

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/SPIRV/disassemble.h>
#include <memory>
#include <sstream>
#include <string>

namespace {

void
spv_init_glslang(struct u_spv *spv)
{
    glslang::InitializeProcess();
    *GetResources() = *GetDefaultResources();
}

EShLanguage
spv_get_stage(const std::string &name)
{
    if (name == "vert")
        return EShLangVertex;
    else if (name == "tesc")
        return EShLangTessControl;
    else if (name == "tese")
        return EShLangTessEvaluation;
    else if (name == "geom")
        return EShLangGeometry;
    else if (name == "frag")
        return EShLangFragment;
    else if (name == "comp")
        return EShLangCompute;
    else if (name == "rgen")
        return EShLangRayGen;
    else if (name == "rint")
        return EShLangIntersect;
    else if (name == "rahit")
        return EShLangAnyHit;
    else if (name == "rchit")
        return EShLangClosestHit;
    else if (name == "rmiss")
        return EShLangMiss;
    else if (name == "rcall")
        return EShLangCallable;
    else if (name == "task")
        return EShLangTask;
    else if (name == "mesh")
        return EShLangMesh;
    else
        spv_die("bad stage name %s", name.c_str());
}

} // namespace

void
spv_init(struct u_spv *spv, const struct spv_init_params *params)
{
    memset(spv, 0, sizeof(*spv));
    if (params)
        spv->params = *params;

    spv_init_glslang(spv);
}

void
spv_cleanup(struct u_spv *spv)
{
    glslang::FinalizeProcess();
}

void *
spv_compile_file(struct u_spv *spv, const char *filename, size_t *size)
{
    const glslang::EShSource lang = glslang::EShSourceGlsl;
    const glslang::EShClient client = glslang::EShClientVulkan;
    const glslang::EShTargetClientVersion client_ver = glslang::EShTargetVulkan_1_2;
    const int dialect_ver = 100;
    const glslang::EShTargetLanguage target_lang = glslang::EShTargetSpv;
    const glslang::EShTargetLanguageVersion target_ver = glslang::EShTargetSpv_1_5;
    const EShMessages messages =
        static_cast<EShMessages>(EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules);

    size_t src_size;
    const char *src_ptr = static_cast<const char *>(u_map_file(filename, &src_size));

    const char *suffix = strrchr(filename, '.');
    if (!suffix)
        spv_die("%s has no suffix", filename);
    const EShLanguage stage = spv_get_stage(++suffix);

    glslang::TShader sh(stage);
    sh.setEnvInput(lang, stage, client, dialect_ver);
    sh.setEnvClient(client, client_ver);
    sh.setEnvTarget(target_lang, target_ver);
    sh.setStringsWithLengthsAndNames(&src_ptr, NULL, &filename, 1);

    if (!sh.parse(GetResources(), dialect_ver, true, messages))
        spv_die("failed to parse %s: %s", filename, sh.getInfoLog());

    u_unmap_file(src_ptr, src_size);

    glslang::TProgram prog;
    prog.addShader(&sh);

    if (!prog.link(messages))
        spv_die("failed to link %s: %s", filename, prog.getInfoLog());

    prog.mapIO();

    std::vector<unsigned int> spirv;
    spv::SpvBuildLogger logger;
    glslang::SpvOptions opts;

    glslang::GlslangToSpv(*prog.getIntermediate(stage), spirv, &logger, &opts);
    const std::string info_log = logger.getAllMessages();
    if (!info_log.empty())
        spv_die("failed to transpile to spirv: %s", info_log.c_str());

    *size = sizeof(spirv[0]) * spirv.size();
    void *out = malloc(*size);
    if (!out)
        spv_die("failed to alloc spirv");
    memcpy(out, spirv.data(), *size);

    return out;
}

void
spv_dump(struct u_spv *spv, const void *spirv, size_t size)
{
    std::vector<unsigned int> src(static_cast<const unsigned int *>(spirv),
                                  static_cast<const unsigned int *>(spirv + size));

    std::ostringstream disasm;
    spv::Disassemble(disasm, src);
    spv_log("%s", disasm.str().c_str());
}
