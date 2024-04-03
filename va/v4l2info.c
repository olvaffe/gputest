/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "v4l2util.h"

struct bitmask_desc {
    uint64_t bitmask;
    const char *str;
};

static const char *
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

    return str;
}

static const char *
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

    return bitmask_to_str(caps, cap_descs, ARRAY_SIZE(cap_descs), str, size);
}

static const char *
v4l2_ctrl_class_to_str(uint32_t cls)
{
    /* clang-format off */
    switch (cls) {
#define CASE(c) case V4L2_CTRL_CLASS_ ##c: return #c
    CASE(USER);
    CASE(CODEC);
    CASE(CAMERA);
    CASE(FM_TX);
    CASE(FLASH);
    CASE(JPEG);
    CASE(IMAGE_SOURCE);
    CASE(IMAGE_PROC);
    CASE(DV);
    CASE(FM_RX);
    CASE(RF_TUNER);
    CASE(DETECT);
    CASE(CODEC_STATELESS);
    CASE(COLORIMETRY);
    default: return "UNKNOWN";
#undef CASE
    }
    /* clang-format on */
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

static const char *
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

    return bitmask_to_str(flags, ctrl_flag_descs, ARRAY_SIZE(ctrl_flag_descs), str, size);
}

static const char *
v4l2_buf_type_to_str(enum v4l2_buf_type type)
{
    /* clang-format off */
    switch (type) {
#define CASE(t) case V4L2_BUF_TYPE_ ##t: return #t
	CASE(VIDEO_CAPTURE);
	CASE(VIDEO_OUTPUT);
	CASE(VIDEO_OVERLAY);
	CASE(VBI_CAPTURE);
	CASE(VBI_OUTPUT);
	CASE(SLICED_VBI_CAPTURE);
	CASE(SLICED_VBI_OUTPUT);
	CASE(VIDEO_OUTPUT_OVERLAY);
	CASE(VIDEO_CAPTURE_MPLANE);
	CASE(VIDEO_OUTPUT_MPLANE);
	CASE(SDR_CAPTURE);
	CASE(SDR_OUTPUT);
	CASE(META_CAPTURE);
	CASE(META_OUTPUT);
    default: return "UNKNOWN";
#undef CASE
    }
    /* clang-format on */
}

static const char *
v4l2_fmt_flag_to_str(uint32_t flags, char *str, size_t size)
{
    static const struct bitmask_desc fmt_flag_descs[] = {
    /* clang-format off */
#define FLAG(f) { .bitmask = V4L2_FMT_FLAG_ ##f, .str = #f }
        FLAG(COMPRESSED),
        FLAG(EMULATED),
        FLAG(CONTINUOUS_BYTESTREAM),
        FLAG(DYN_RESOLUTION),
        FLAG(ENC_CAP_FRAME_INTERVAL),
        FLAG(CSC_COLORSPACE),
        FLAG(CSC_XFER_FUNC),
        FLAG(CSC_YCBCR_ENC),
        FLAG(CSC_QUANTIZATION),
#undef FLAG
        /* clang-format on */
    };

    return bitmask_to_str(flags, fmt_flag_descs, ARRAY_SIZE(fmt_flag_descs), str, size);
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
    v4l2_log("  caps: %s", v4l2_cap_to_str(cap->capabilities, str, sizeof(str)));
    v4l2_log("  device caps: %s", v4l2_cap_to_str(cap->device_caps, str, sizeof(str)));
}

static void
v4l2_dump_ctrl(struct v4l2 *v4l2, uint32_t idx)
{
    const struct v4l2_queryctrl *ctrl = &v4l2->ctrls[idx];

    char str[256];
    v4l2_log("'%s' %s ctrl: type %s, flags %s", ctrl->name,
             v4l2_ctrl_class_to_str(V4L2_CTRL_ID2CLASS(ctrl->id)),
             v4l2_ctrl_type_to_str(ctrl->type),
             v4l2_ctrl_flag_to_str(ctrl->flags, str, sizeof(str)));
    v4l2_log("  min/max/step/default: %d/%d/%d/%d", ctrl->minimum, ctrl->maximum, ctrl->step,
             ctrl->default_value);
}

static void
v4l2_dump_formats(struct v4l2 *v4l2)
{
    const enum v4l2_buf_type all_types[] = {
        V4L2_BUF_TYPE_VIDEO_CAPTURE,        V4L2_BUF_TYPE_VIDEO_OUTPUT,
        V4L2_BUF_TYPE_VIDEO_OVERLAY,        V4L2_BUF_TYPE_VBI_CAPTURE,
        V4L2_BUF_TYPE_VBI_OUTPUT,           V4L2_BUF_TYPE_SLICED_VBI_CAPTURE,
        V4L2_BUF_TYPE_SLICED_VBI_OUTPUT,    V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY,
        V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
        V4L2_BUF_TYPE_SDR_CAPTURE,          V4L2_BUF_TYPE_SDR_OUTPUT,
        V4L2_BUF_TYPE_META_CAPTURE,         V4L2_BUF_TYPE_META_OUTPUT,
    };

    for (uint32_t i = 0; i < ARRAY_SIZE(all_types); i++) {
        const enum v4l2_buf_type type = all_types[i];
        uint32_t count;
        struct v4l2_fmtdesc *descs = v4l2_enumerate_formats(v4l2, all_types[i], &count);
        if (!count)
            continue;

        v4l2_log("%s buf type:", v4l2_buf_type_to_str(type));
        for (uint32_t j = 0; j < count; j++) {
            struct v4l2_fmtdesc *desc = &descs[j];

            char str[256];
            v4l2_log("  '%.*s': %s, flags %s, mbus %d", 4, (const char *)&desc->pixelformat,
                     desc->description, v4l2_fmt_flag_to_str(desc->flags, str, sizeof(str)),
                     desc->mbus_code);
        }
        free(descs);
    }
}

static void
v4l2_dump(struct v4l2 *v4l2)
{
    v4l2_dump_cap(v4l2);

    for (uint32_t i = 0; i < v4l2->ctrl_count; i++)
        v4l2_dump_ctrl(v4l2, i);

    v4l2_dump_formats(v4l2);
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
