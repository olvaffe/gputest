/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef SPVUTIL_H
#define SPVUTIL_H

#include "util.h"

#include <glslang/Include/glslang_c_interface.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct spv_init_params {
    glslang_messages_t messages;
};

struct spv {
    struct spv_init_params params;
};

struct spv_program {
    glslang_shader_t *glsl_sh;
    glslang_program_t *glsl_prog;

    const void *spirv;
    size_t size;
};

static inline void PRINTFLIKE(1, 2) spv_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_logv("SPV", format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(1, 2) NORETURN spv_die(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_diev("SPV", format, ap);
    va_end(ap);
}

void
spv_init(struct spv *spv, const struct spv_init_params *params);

void
spv_cleanup(struct spv *spv);

glslang_stage_t
spv_guess_stage(struct spv *spv, const char *filename);

struct spv_program *
spv_create_program_from_shader(struct spv *spv, glslang_stage_t stage, const char *filename);

void
spv_destroy_program(struct spv *spv, struct spv_program *prog);

void
spv_disasm_program(struct spv *spv, struct spv_program *prog);

#if defined(__cplusplus)
}
#endif

#endif /* SPVUTIL_H */
