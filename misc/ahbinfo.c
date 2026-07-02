/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "androidutil.h"

static const struct {
    enum AHardwareBuffer_UsageFlags bits;
    const char *name;
} ahb_usage_table[] = {
    { AHARDWAREBUFFER_USAGE_CPU_READ_RARELY, "cpu-r" },
    { AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY, "cpu-w" },
    { AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE, "gpu-r" },
    { AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER, "gpu-w" },
    { AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY, "comp-overlay" },
    { AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT, "prot" },
    { 1ull << 15 /* COMPOSER_CURSOR */, "comp-cursor" },
    { AHARDWAREBUFFER_USAGE_VIDEO_ENCODE, "vid-enc" },
    { 1ull << 17 /* CAMERA_OUTPUT */, "cam-w" },
    { 1ull << 18 /* CAMERA_INPUT */, "cam-r" },
    { 1ull << 22 /* VIDEO_DECODER */, "vid-dec" },
    { AHARDWAREBUFFER_USAGE_SENSOR_DIRECT_DATA, "sensor" },
    { AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER, "gpu-buf" },
    { AHARDWAREBUFFER_USAGE_GPU_CUBE_MAP, "gpu-cube" },
    { AHARDWAREBUFFER_USAGE_GPU_MIPMAP_COMPLETE, "gpu-mip" },
    { 1ull << 27 /* HW_IMAGE_ENCODER */, "img-enc" },
    { 1ull << 32 /* FRONT_BUFFER */, "front" },
    { 1ull << 33 /* NPU */, "img-enc" },
};

static void
ahb_get_usage_str(uint64_t bits, char *str, size_t size)
{
    int len = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(ahb_usage_table); i++) {
        if (bits & ahb_usage_table[i].bits) {
            len += snprintf(str + len, size - len, "%s|", ahb_usage_table[i].name);
            bits &= ~ahb_usage_table[i].bits;
        }
    }

    if (bits)
        snprintf(str + len, size - len, "0x%" PRIx64, bits);
    else if (len)
        str[len - 1] = '\0';
    else
        snprintf(str + len, size - len, "none");
}

static bool ahb_is_supported(enum AHardwareBuffer_Format fmt, uint64_t usage)
{
    const AHardwareBuffer_Desc desc = {
        .width = 64,
        .height = fmt == AHARDWAREBUFFER_FORMAT_BLOB ? 1 : 64,
        .layers = 1,
        .format = fmt,
        .usage = usage,
    };

    return AHardwareBuffer_isSupported(&desc);
}

static void
ahb_dump(struct android *android)
{
    for (uint32_t i = 0; i < ARRAY_SIZE(android_ahb_format_table); i++) {
        enum AHardwareBuffer_Format fmt = android_ahb_format_table[i].ahb_format;
        const char *name = android_ahb_format_table[i].name;

        uint64_t bits = 0;
        for (uint32_t j = 0; j < ARRAY_SIZE(ahb_usage_table); j++) {
            enum AHardwareBuffer_UsageFlags usage = ahb_usage_table[j].bits;
            if (ahb_is_supported(fmt, usage))
                bits |= usage;
        }

        char buf[1024];
        ahb_get_usage_str(bits, buf, sizeof(buf));

        android_log("%s: %s", name, buf);
    }
}

int
main(int argc, char **argv)
{
    struct android android;
    android_init(&android, NULL);
    ahb_dump(&android);
    android_cleanup(&android);

    return 0;
}
