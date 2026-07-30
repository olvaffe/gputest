/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef SPVUTIL_H
#define SPVUTIL_H

#include "util.h"

#include <spirv/unified1/spirv.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct spv_init_params {
    int dummy;
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
    SpvExecutionModel execution_model;
    char *entrypoint;

    uint32_t set_count;
    struct spv_program_reflection_set *sets;
};

struct spv_program {
    void *spirv;
    size_t size;

    struct spv_program_reflection reflection;
};

#define spv_log(format, ...) u_log("SPV", format __VA_OPT__(, ) __VA_ARGS__)
#define spv_die(format, ...) u_die("SPV", format __VA_OPT__(, ) __VA_ARGS__)

void
spv_init(struct spv *spv, const struct spv_init_params *params);

void
spv_cleanup(struct spv *spv);

struct spv_program *
spv_create_program(struct spv *spv, const char *filename);

void
spv_destroy_program(struct spv *spv, struct spv_program *prog);

void
spv_disasm_program(struct spv *spv, struct spv_program *prog);

#if defined(__cplusplus)
}
#endif

#endif /* SPVUTIL_H */
