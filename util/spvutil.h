/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef SPVUTIL_H
#define SPVUTIL_H

#include "util.h"

#include <glslang/Include/glslang_c_interface.h>

#define SPV_STAGE_KERNEL GLSLANG_STAGE_COUNT

#if defined(__cplusplus)
extern "C" {
#endif

struct spv_init_params {
    glslang_messages_t messages;
};

struct spv {
    struct spv_init_params params;
};

struct spv_program_reflection_binding {
    uint32_t binding;
    int type;
    uint32_t count;
};

struct spv_program_reflection_set {
    uint32_t binding_count;
    struct spv_program_reflection_binding *bindings;
};

struct spv_program_reflection {
    char *entrypoint;

    uint32_t set_count;
    struct spv_program_reflection_set *sets;
};

struct spv_program {
    /* GLSLANG_STAGE_* or SPV_STAGE_* */
    int stage;
    void *spirv;
    size_t size;

    struct spv_program_reflection reflection;
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

int
spv_guess_stage(struct spv *spv, const char *filename);

struct spv_program *
spv_create_program(struct spv *spv, int stage, const char *filename);

void
spv_destroy_program(struct spv *spv, struct spv_program *prog);

void
spv_reflect_program(struct spv *spv, struct spv_program *prog);

void
spv_disasm_program(struct spv *spv, struct spv_program *prog);

#if defined(__cplusplus)
}
#endif

#endif /* SPVUTIL_H */
