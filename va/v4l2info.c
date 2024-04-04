/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "v4l2util.h"

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
v4l2_dump_ctrls(struct v4l2 *v4l2)
{
    uint32_t count;
    struct v4l2_queryctrl *ctrls = v4l2_enumerate_controls(v4l2, &count);

    for (uint32_t i = 0; i < count; i++) {
        const struct v4l2_queryctrl *ctrl = &ctrls[i];

        char str[256];
        v4l2_log("'%s' %s ctrl: type %s, flags %s", ctrl->name,
                 v4l2_ctrl_class_to_str(V4L2_CTRL_ID2CLASS(ctrl->id)),
                 v4l2_ctrl_type_to_str(ctrl->type),
                 v4l2_ctrl_flag_to_str(ctrl->flags, str, sizeof(str)));
        v4l2_log("  min/max/step/default: %d/%d/%d/%d", ctrl->minimum, ctrl->maximum, ctrl->step,
                 ctrl->default_value);
    }

    free(ctrls);
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

            uint32_t size_count;
            struct v4l2_frmsizeenum *sizes =
                v4l2_enumerate_frame_sizes(v4l2, desc->pixelformat, &size_count);
            for (uint32_t k = 0; k < size_count; k++) {
                struct v4l2_frmsizeenum *size = &sizes[k];
                switch (size->type) {
                case V4L2_FRMSIZE_TYPE_DISCRETE: {
                    uint32_t interval_count;
                    struct v4l2_frmivalenum *intervals = v4l2_enumerate_frame_intervals(
                        v4l2, size->discrete.width, size->discrete.height, desc->pixelformat,
                        &interval_count);
                    for (uint32_t l = 0; l < interval_count; l++) {
                        struct v4l2_frmivalenum *interval = &intervals[l];
                        if (interval->type == V4L2_FRMIVAL_TYPE_DISCRETE)
                            v4l2_log("    %dx%d, interval %d/%d", interval->width,
                                     interval->height, interval->discrete.numerator,
                                     interval->discrete.denominator);
                        else
                            v4l2_log("    %dx%d", interval->width, interval->height);
                    }
                    free(intervals);
                } break;
                case V4L2_FRMSIZE_TYPE_CONTINUOUS:
                case V4L2_FRMSIZE_TYPE_STEPWISE:
                default:
                    v4l2_log("    type %d", size->type);
                    break;
                }
            }
            free(sizes);
        }
        free(descs);
    }
}

static void
v4l2_dump_inputs(struct v4l2 *v4l2)
{
    uint32_t count;
    struct v4l2_input *inputs = v4l2_enumerate_inputs(v4l2, &count);
    if (!count)
        return;

    for (uint32_t i = 0; i < count; i++) {
        struct v4l2_input *input = &inputs[i];

        v4l2_log("input #%d: %s, type %s, audioset 0x%x, tuner %d, std %d, status %d, caps 0x%x",
                 input->index, input->name, v4l2_input_type_to_str(input->type), input->audioset,
                 input->tuner, (int)input->std, input->status, input->capabilities);
    }
    free(inputs);
}

static void
v4l2_dump_current_format(struct v4l2 *v4l2)
{
    if (!(v4l2->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
        return;

    struct v4l2_format fmt;
    v4l2_vidioc_g_fmt(v4l2, V4L2_BUF_TYPE_VIDEO_CAPTURE, &fmt);
    const struct v4l2_pix_format *pix = &fmt.fmt.pix;
    v4l2_log("current format: '%.*s', %dx%d, field %d, pitch %d, size %d, colorspace %s", 4,
             (const char *)&pix->pixelformat, pix->width, pix->height, pix->field,
             pix->bytesperline, pix->sizeimage, v4l2_colorspace_to_str(pix->colorspace));
    if (pix->priv == V4L2_PIX_FMT_PRIV_MAGIC) {
        v4l2_log("  flags 0x%x, ycbcr enc %s quant %d, xfer %s", pix->flags,
                 v4l2_ycbcr_enc_to_str(pix->ycbcr_enc), pix->quantization,
                 v4l2_xfer_func_to_str(pix->xfer_func));
    }

    if (!(v4l2->cap.capabilities & V4L2_CAP_STREAMING))
        return;

    struct v4l2_create_buffers buf;
    v4l2_vidioc_create_bufs(v4l2, V4L2_MEMORY_MMAP, &fmt, &buf);

    char str[256];
    v4l2_log("current bufs: count %d, caps %s", buf.index,
             v4l2_buf_cap_to_str(buf.capabilities, str, sizeof(str)));
}

static void
v4l2_dump(struct v4l2 *v4l2)
{
    v4l2_dump_cap(v4l2);
    v4l2_dump_ctrls(v4l2);
    v4l2_dump_formats(v4l2);
    v4l2_dump_inputs(v4l2);

    v4l2_dump_current_format(v4l2);
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
