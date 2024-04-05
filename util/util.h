/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef UTIL_H
#define UTIL_H

#include "drm/drm_fourcc.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PRINTFLIKE(f, a) __attribute__((format(printf, f, a)))
#define NORETURN __attribute__((noreturn))

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define ALIGN(v, a) (((v) + (a) - 1) & ~((a) - 1))
#define DIV_ROUND_UP(v, d) (((v) + (d) - 1) / (d))

static inline void
u_logv(const char *tag, const char *format, va_list ap)
{
    printf("%s: ", tag);
    vprintf(format, ap);
    printf("\n");
}

static inline void NORETURN
u_diev(const char *tag, const char *format, va_list ap)
{
    u_logv(tag, format, ap);
    abort();
}

static inline void PRINTFLIKE(2, 3) u_log(const char *tag, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_logv(tag, format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(2, 3) NORETURN u_die(const char *tag, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_diev(tag, format, ap);
    va_end(ap);
}

struct u_bitmask_desc {
    uint64_t bitmask;
    const char *str;
};

static inline const char *
u_bitmask_to_str(
    uint64_t bitmask, const struct u_bitmask_desc *descs, uint32_t count, char *str, size_t size)
{
    int len = 0;
    for (uint32_t i = 0; i < count; i++) {
        const struct u_bitmask_desc *desc = &descs[i];
        if (bitmask & desc->bitmask) {
            len += snprintf(str + len, size - len, "%s|", desc->str);
            bitmask &= ~desc->bitmask;
        }
    }

    if (bitmask)
        snprintf(str + len, size - len, "0x%" PRIx64, bitmask);
    else if (len)
        str[len - 1] = '\0';
    else
        snprintf(str + len, size - len, "none");

    return str;
}

static inline uint32_t
u_minify(uint32_t base, uint32_t level)
{
    const uint32_t val = base >> level;
    return val ? val : 1;
}

static inline uint64_t
u_now(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts))
        return 0;

    const uint64_t ns = 1000000000ull;
    return ns * ts.tv_sec + ts.tv_nsec;
}

static inline void
u_sleep(uint32_t ms)
{
    const struct timespec ts = {
        .tv_sec = ms / 1000,
        .tv_nsec = (ms % 1000) * 1000000,
    };

    const int ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
    if (ret)
        u_die("util", "failed to sleep");
}

#endif /* UTIL_H */
