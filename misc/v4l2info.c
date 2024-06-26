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

    v4l2_log("ctrl count: %d", count);
    for (uint32_t i = 0; i < count; i++) {
        const struct v4l2_queryctrl *ctrl = &ctrls[i];

        char str[256];
        v4l2_log("  %s '%s': type %s, flags %s",
                 v4l2_ctrl_class_to_str(V4L2_CTRL_ID2CLASS(ctrl->id)), ctrl->name,
                 v4l2_ctrl_type_to_str(ctrl->type),
                 v4l2_ctrl_flag_to_str(ctrl->flags, str, sizeof(str)));
        v4l2_log("    min/max/step/default: %d/%d/%d/%d", ctrl->minimum, ctrl->maximum,
                 ctrl->step, ctrl->default_value);
    }

    free(ctrls);
}

static void
v4l2_dump_formats(struct v4l2 *v4l2)
{
    uint32_t count;
    enum v4l2_buf_type *types = v4l2_enumerate_buf_types(v4l2, &count);

    for (uint32_t i = 0; i < count; i++) {
        const enum v4l2_buf_type type = types[i];
        uint32_t count;
        struct v4l2_fmtdesc *descs = v4l2_enumerate_formats(v4l2, type, &count);
        if (!count)
            continue;

        v4l2_log("%s format count: %d", v4l2_buf_type_to_str(type), count);
        for (uint32_t j = 0; j < count; j++) {
            struct v4l2_fmtdesc *desc = &descs[j];

            char str[256];
            v4l2_log("  '%.*s': %s, flags %s", 4, (const char *)&desc->pixelformat,
                     desc->description, v4l2_fmt_flag_to_str(desc->flags, str, sizeof(str)));

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

    free(types);
}

static void
v4l2_dump_inputs(struct v4l2 *v4l2)
{
    uint32_t count;
    struct v4l2_input *inputs = v4l2_enumerate_inputs(v4l2, &count);
    if (!count)
        return;

    v4l2_log("input count: %d", count);
    for (uint32_t i = 0; i < count; i++) {
        struct v4l2_input *input = &inputs[i];

        v4l2_log(
            "  input #%d: %s, type %s, audioset 0x%x, tuner %d, std %d, status %d, caps 0x%x",
            input->index, input->name, v4l2_input_type_to_str(input->type), input->audioset,
            input->tuner, (int)input->std, input->status, input->capabilities);
    }
    free(inputs);
}

static void
v4l2_dump_outputs(struct v4l2 *v4l2)
{
    uint32_t count;
    struct v4l2_output *outputs = v4l2_enumerate_outputs(v4l2, &count);
    if (!count)
        return;

    v4l2_log("output count: %d", count);
    for (uint32_t i = 0; i < count; i++) {
        struct v4l2_output *output = &outputs[i];

        v4l2_log("  output #%d: %s, type %s, audioset 0x%x, modulator %d, std %d, caps 0x%x",
                 output->index, output->name, v4l2_output_type_to_str(output->type),
                 output->audioset, output->modulator, (int)output->std, output->capabilities);
    }
    free(outputs);
}

static void
v4l2_dump_current_states(struct v4l2 *v4l2)
{
    v4l2_log("current states:");

    if (v4l2_vidioc_enuminput_count(v4l2) > 0)
        v4l2_log("  input: %d", v4l2_vidioc_g_input(v4l2));
    if (v4l2_vidioc_enumoutput_count(v4l2) > 0)
        v4l2_log("  output: %d", v4l2_vidioc_g_output(v4l2));

    uint32_t count;
    enum v4l2_buf_type *types = v4l2_enumerate_buf_types(v4l2, &count);

    for (uint32_t i = 0; i < count; i++) {
        const enum v4l2_buf_type type = types[i];
        v4l2_log("  %s:", v4l2_buf_type_to_str(type));

        bool is_capture = false;
        bool is_mplane = false;
        switch (type) {
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
            is_capture = true;
            break;
        case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
            is_capture = true;
            is_mplane = true;
            break;
        case V4L2_BUF_TYPE_VIDEO_OUTPUT:
            break;
        case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
            is_mplane = true;
            break;
        default:
            v4l2_die("unexpected buf type");
            break;
        }

        struct v4l2_format fmt;
        v4l2_vidioc_g_fmt(v4l2, type, &fmt);
        if (is_mplane) {
            const struct v4l2_pix_format_mplane *mp = &fmt.fmt.pix_mp;
            v4l2_log("    format: '%.*s', %dx%d, field %d, colorspace %s", 4,
                     (const char *)&mp->pixelformat, mp->width, mp->height, mp->field,
                     v4l2_colorspace_to_str(mp->colorspace));
            v4l2_log("      flags 0x%x, ycbcr enc %s quant %d, xfer %s", mp->flags,
                     v4l2_ycbcr_enc_to_str(mp->ycbcr_enc), mp->quantization,
                     v4l2_xfer_func_to_str(mp->xfer_func));
            for (uint32_t j = 0; j < mp->num_planes; j++) {
                const struct v4l2_plane_pix_format *plane = &mp->plane_fmt[j];
                v4l2_log("      plane %d: pitch %d, size %d", j, plane->bytesperline,
                         plane->sizeimage);
            }
        } else {
            const struct v4l2_pix_format *pix = &fmt.fmt.pix;
            v4l2_log("    format: '%.*s', %dx%d, field %d, pitch %d, size %d, colorspace %s", 4,
                     (const char *)&pix->pixelformat, pix->width, pix->height, pix->field,
                     pix->bytesperline, pix->sizeimage, v4l2_colorspace_to_str(pix->colorspace));
            if (pix->priv == V4L2_PIX_FMT_PRIV_MAGIC) {
                v4l2_log("      flags 0x%x, ycbcr enc %s, quant %d, xfer %s", pix->flags,
                         v4l2_ycbcr_enc_to_str(pix->ycbcr_enc), pix->quantization,
                         v4l2_xfer_func_to_str(pix->xfer_func));
            }
        }

        struct v4l2_streamparm parm;
        v4l2_vidioc_g_parm(v4l2, type, &parm);
        if (is_capture) {
            const struct v4l2_captureparm *capture = &parm.parm.capture;
            v4l2_log(
                "    capture parameters: cap 0x%x, mode 0x%x, interval %d/%d, ext %d, readbuf %d",
                capture->capability, capture->capturemode, capture->timeperframe.numerator,
                capture->timeperframe.denominator, capture->extendedmode, capture->readbuffers);
        } else {
            const struct v4l2_outputparm *output = &parm.parm.output;
            v4l2_log(
                "    output parameters: cap 0x%x, mode 0x%x, interval %d/%d, ext %d, writebuf %d",
                output->capability, output->outputmode, output->timeperframe.numerator,
                output->timeperframe.denominator, output->extendedmode, output->writebuffers);
        }

        if (!(v4l2->cap.device_caps & V4L2_CAP_STREAMING))
            continue;

        struct v4l2_create_buffers buf;
        v4l2_vidioc_create_bufs(v4l2, V4L2_MEMORY_MMAP, &fmt, &buf);

        char str[256];
        v4l2_log("    bufs: count %d, caps %s", buf.index,
                 v4l2_buf_cap_to_str(buf.capabilities, str, sizeof(str)));
    }

    free(types);
}

static void
v4l2_dump(struct v4l2 *v4l2)
{
    v4l2_dump_cap(v4l2);
    v4l2_dump_ctrls(v4l2);
    v4l2_dump_formats(v4l2);
    v4l2_dump_inputs(v4l2);
    v4l2_dump_outputs(v4l2);

    v4l2_dump_current_states(v4l2);
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
