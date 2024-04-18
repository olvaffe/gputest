/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef SPVUTIL_H
#define SPVUTIL_H

#include "util.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct spv_init_params {
    int unused;
};

struct u_spv {
    struct spv_init_params params;
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
spv_init(struct u_spv *spv, const struct spv_init_params *params);

void
spv_cleanup(struct u_spv *spv);

void *
spv_compile_file(struct u_spv *spv, const char *filename, size_t *size);

void
spv_dump(struct u_spv *spv, const void *spirv, size_t size);

#if defined(__cplusplus)
}
#endif

#endif /* SPVUTIL_H */
