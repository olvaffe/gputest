/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "v4l2util.h"

struct bitmask_desc {
    uint64_t bitmask;
    const char *str;
};

static void
bitmask_to_str(
    uint64_t bitmask, const struct bitmask_desc *descs, uint32_t count, char *str, size_t size)
{
    int len = 0;
    for (uint32_t i = 0; i < count; i++) {
        const struct bitmask_desc *desc = &descs[i];
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
}

static void
v4l2_cap_to_str(uint32_t caps, char *str, size_t size)
{
    static const struct bitmask_desc cap_descs[] = {
        /* clang-format off */
        { .bitmask = V4L2_CAP_VIDEO_CAPTURE,        .str = "v-cap" },
        { .bitmask = V4L2_CAP_VIDEO_OUTPUT,         .str = "v-out" },
        { .bitmask = V4L2_CAP_VIDEO_OVERLAY,        .str = "v-ovl" },
        { .bitmask = V4L2_CAP_VBI_CAPTURE,          .str = "vbi-cap" },
        { .bitmask = V4L2_CAP_VBI_OUTPUT,           .str = "vbi-out" },
        { .bitmask = V4L2_CAP_SLICED_VBI_CAPTURE,   .str = "svbi-cap" },
        { .bitmask = V4L2_CAP_SLICED_VBI_OUTPUT,    .str = "svbi-out" },
        { .bitmask = V4L2_CAP_RDS_CAPTURE,          .str = "rds-cap" },
        { .bitmask = V4L2_CAP_VIDEO_OUTPUT_OVERLAY, .str = "v-out-ovl" },
        { .bitmask = V4L2_CAP_HW_FREQ_SEEK,         .str = "freq-seek" },
        { .bitmask = V4L2_CAP_RDS_OUTPUT,           .str = "rds-out" },
        { .bitmask = V4L2_CAP_VIDEO_CAPTURE_MPLANE, .str = "v-cap-mp" },
        { .bitmask = V4L2_CAP_VIDEO_OUTPUT_MPLANE,  .str = "v-out-mp" },
        { .bitmask = V4L2_CAP_VIDEO_M2M_MPLANE,     .str = "v-m2m-mp" },
        { .bitmask = V4L2_CAP_VIDEO_M2M,            .str = "v-m2m" },
        { .bitmask = V4L2_CAP_TUNER,                .str = "tuner" },
        { .bitmask = V4L2_CAP_AUDIO,                .str = "audio" },
        { .bitmask = V4L2_CAP_RADIO,                .str = "radio" },
        { .bitmask = V4L2_CAP_MODULATOR,            .str = "modulator" },
        { .bitmask = V4L2_CAP_SDR_CAPTURE,          .str = "sdr-cap" },
        { .bitmask = V4L2_CAP_EXT_PIX_FORMAT,       .str = "ext-pix-fmt" },
        { .bitmask = V4L2_CAP_SDR_OUTPUT,           .str = "sdr-out" },
        { .bitmask = V4L2_CAP_META_CAPTURE,         .str = "meta-cap" },
        { .bitmask = V4L2_CAP_READWRITE,            .str = "rw" },
        { .bitmask = V4L2_CAP_STREAMING,            .str = "stream" },
        { .bitmask = V4L2_CAP_META_OUTPUT,          .str = "meta-out" },
        { .bitmask = V4L2_CAP_TOUCH,                .str = "touch" },
        { .bitmask = V4L2_CAP_IO_MC,                .str = "io-mc" },
        { .bitmask = V4L2_CAP_DEVICE_CAPS,          .str = "dev-caps" },
        /* clang-format on */
    };

    bitmask_to_str(caps, cap_descs, ARRAY_SIZE(cap_descs), str, size);
}

static const char *
v4l2_ctrl_type_to_str(enum v4l2_ctrl_type type)
{
    /* clang-format off */
    switch (type) {
#define CASE(t) case V4L2_CTRL_TYPE_ ##t: return #t
    CASE(INTEGER);
    CASE(BOOLEAN);
    CASE(MENU);
    CASE(BUTTON);
    CASE(INTEGER64);
    CASE(CTRL_CLASS);
    CASE(STRING);
    CASE(BITMASK);
    CASE(INTEGER_MENU);
    CASE(U8);
    CASE(U16);
    CASE(U32);
    CASE(AREA);
    CASE(HDR10_CLL_INFO);
    CASE(HDR10_MASTERING_DISPLAY);
    CASE(H264_SPS);
    CASE(H264_PPS);
    CASE(H264_SCALING_MATRIX);
    CASE(H264_SLICE_PARAMS);
    CASE(H264_DECODE_PARAMS);
    CASE(H264_PRED_WEIGHTS);
    CASE(FWHT_PARAMS);
    CASE(VP8_FRAME);
    CASE(MPEG2_QUANTISATION);
    CASE(MPEG2_SEQUENCE);
    CASE(MPEG2_PICTURE);
    CASE(VP9_COMPRESSED_HDR);
    CASE(VP9_FRAME);
    CASE(HEVC_SPS);
    CASE(HEVC_PPS);
    CASE(HEVC_SLICE_PARAMS);
    CASE(HEVC_SCALING_MATRIX);
    CASE(HEVC_DECODE_PARAMS);
    CASE(AV1_SEQUENCE);
    CASE(AV1_TILE_GROUP_ENTRY);
    CASE(AV1_FRAME);
    CASE(AV1_FILM_GRAIN);
    default: return "UNKNOWN";
#undef CASE
    }
    /* clang-format on */
}

static void
v4l2_ctrl_flag_to_str(uint32_t flags, char *str, size_t size)
{
    static const struct bitmask_desc ctrl_flag_descs[] = {
    /* clang-format off */
#define FLAG(f) { .bitmask = V4L2_CTRL_FLAG_ ##f, .str = #f }
        FLAG(DISABLED),
        FLAG(GRABBED),
        FLAG(READ_ONLY),
        FLAG(UPDATE),
        FLAG(INACTIVE),
        FLAG(SLIDER),
        FLAG(WRITE_ONLY),
        FLAG(VOLATILE),
        FLAG(HAS_PAYLOAD),
        FLAG(EXECUTE_ON_WRITE),
        FLAG(MODIFY_LAYOUT),
        FLAG(DYNAMIC_ARRAY),
#undef FLAG
        /* clang-format on */
    };

    bitmask_to_str(flags, ctrl_flag_descs, ARRAY_SIZE(ctrl_flag_descs), str, size);
}

static void
v4l2_dump_cap(struct v4l2 *v4l2)
{
    const struct v4l2_capability *cap = &v4l2->cap;

    v4l2_log("device: %s", v4l2->params.path);
    v4l2_log("  driver: %s", cap->driver);
    v4l2_log("  card: %s", cap->card);
    v4l2_log("  bus: %s", cap->bus_info);
    v4l2_log("  version: 0x%x", cap->version);

    char str[256];
    v4l2_cap_to_str(cap->capabilities, str, sizeof(str));
    v4l2_log("  caps: %s", str);

    v4l2_cap_to_str(cap->device_caps, str, sizeof(str));
    v4l2_log("  device caps: %s", str);
}

static void
v4l2_dump_ctrl(struct v4l2 *v4l2, uint32_t idx)
{
    const struct v4l2_queryctrl *ctrl = &v4l2->ctrls[idx];

    char str[256];
    v4l2_ctrl_flag_to_str(ctrl->flags, str, sizeof(str));
    v4l2_log("ctrl: id 0x%x, name %s, flags %s", ctrl->id, ctrl->name, str);
    v4l2_log("  min/max/step/default: %d/%d/%d/%d, type %s", ctrl->minimum, ctrl->maximum,
             ctrl->step, ctrl->default_value, v4l2_ctrl_type_to_str(ctrl->type));
}

static void
v4l2_dump(struct v4l2 *v4l2)
{
    v4l2_dump_cap(v4l2);

    for (uint32_t i = 0; i < v4l2->ctrl_count; i++)
        v4l2_dump_ctrl(v4l2, i);
}

int
main(int argc, char **argv)
{
    if (argc != 2)
        v4l2_die("usage: %s <device-path>", argv[0]);
    const char *path = argv[1];

    struct v4l2 v4l2;
    const struct v4l2_init_params params = {
        .path = path,
    };
    v4l2_init(&v4l2, &params);
    v4l2_dump(&v4l2);
    v4l2_cleanup(&v4l2);

    return 0;
}
