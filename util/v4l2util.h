/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef V4L2UTIL_H
#define V4L2UTIL_H

#include "util.h"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct v4l2_init_params {
    const char *path;
};

struct v4l2 {
    struct v4l2_init_params params;

    int ret;
    int fd;
    struct v4l2_capability cap;
};

static inline void PRINTFLIKE(1, 2) v4l2_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_logv("V4L2", format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(1, 2) NORETURN v4l2_die(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_diev("V4L2", format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(2, 3) v4l2_check(struct v4l2 *v4l2, const char *format, ...)
{
    if (v4l2->ret >= 0)
        return;

    va_list ap;
    va_start(ap, format);
    u_diev("V4L2", format, ap);
    va_end(ap);
}

static inline void
v4l2_vidioc_querycap(struct v4l2 *v4l2, struct v4l2_capability *args)
{
    v4l2->ret = ioctl(v4l2->fd, VIDIOC_QUERYCAP, args);
    v4l2_check(v4l2, "failed to VIDIOC_QUERYCAP");
}

static inline uint32_t
v4l2_vidioc_enum_fmt_count(struct v4l2 *v4l2, enum v4l2_buf_type type)
{
    for (uint32_t i = 0;; i++) {
        struct v4l2_fmtdesc args = {
            .index = i,
            .type = type,
        };
        if (ioctl(v4l2->fd, VIDIOC_ENUM_FMT, &args))
            return i;
    }
}

static inline void
v4l2_vidioc_enum_fmt(struct v4l2 *v4l2,
                     enum v4l2_buf_type type,
                     uint32_t index,
                     struct v4l2_fmtdesc *args)
{
    *args = (struct v4l2_fmtdesc){
        .index = index,
        .type = type,
    };
    v4l2->ret = ioctl(v4l2->fd, VIDIOC_ENUM_FMT, args);
    v4l2_check(v4l2, "VIDIOC_ENUM_FMT");
}

static inline uint32_t
v4l2_vidioc_enum_framesizes_count(struct v4l2 *v4l2, uint32_t format)
{
    for (uint32_t i = 0;; i++) {
        struct v4l2_frmsizeenum args = {
            .index = i,
            .pixel_format = format,
        };
        if (ioctl(v4l2->fd, VIDIOC_ENUM_FRAMESIZES, &args))
            return i;
    }
}

static inline void
v4l2_vidioc_enum_framesizes(struct v4l2 *v4l2,
                            uint32_t format,
                            uint32_t index,
                            struct v4l2_frmsizeenum *args)
{
    *args = (struct v4l2_frmsizeenum){
        .index = index,
        .pixel_format = format,
    };
    v4l2->ret = ioctl(v4l2->fd, VIDIOC_ENUM_FRAMESIZES, args);
    v4l2_check(v4l2, "VIDIOC_ENUM_FRAMESIZES");
}

static inline uint32_t
v4l2_vidioc_enum_frameintervals_count(struct v4l2 *v4l2, uint32_t format, uint32_t width, uint32_t
        height)
{
    for (uint32_t i = 0;; i++) {
        struct v4l2_frmivalenum args = {
            .index = i,
            .pixel_format = format,
            .width = width,
            .height = height,
        };
        if (ioctl(v4l2->fd, VIDIOC_ENUM_FRAMEINTERVALS, &args))
            return i;
    }
}

static inline void
v4l2_vidioc_enum_frameintervals(struct v4l2 *v4l2,
                            uint32_t format,
                            uint32_t width,
                            uint32_t height,
                            uint32_t index,
                            struct v4l2_frmivalenum *args)
{
    *args = (struct v4l2_frmivalenum){
        .index = index,
        .pixel_format = format,
        .width = width,
        .height = height,
    };
    v4l2->ret = ioctl(v4l2->fd, VIDIOC_ENUM_FRAMEINTERVALS, args);
    v4l2_check(v4l2, "VIDIOC_ENUM_FRAMEINTERVALS");
}

static inline uint32_t
v4l2_vidioc_enuminput_count(struct v4l2 *v4l2)
{
    for (uint32_t i = 0;; i++) {
        struct v4l2_input args = {
            .index = i,
        };
        if (ioctl(v4l2->fd, VIDIOC_ENUMINPUT, &args))
            return i;
    }
}

static inline void
v4l2_vidioc_enuminput(struct v4l2 *v4l2, uint32_t index, struct v4l2_input *args)
{
    *args = (struct v4l2_input){
        .index = index,
    };
    v4l2->ret = ioctl(v4l2->fd, VIDIOC_ENUMINPUT, args);
    v4l2_check(v4l2, "VIDIOC_ENUMINPUT");
}

static inline uint32_t
v4l2_vidioc_queryctrl_count(struct v4l2 *v4l2)
{
    const uint32_t next_flags = V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;
    struct v4l2_queryctrl args = {
        .id = 0,
    };
    for (uint32_t i = 0;; i++) {
        args.id |= next_flags;
        if (ioctl(v4l2->fd, VIDIOC_QUERYCTRL, &args))
            return i;
    }
}

static inline void
v4l2_vidioc_queryctrl_next(struct v4l2 *v4l2, uint32_t id, struct v4l2_queryctrl *args)
{
    const uint32_t next_flags = V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;
    *args = (struct v4l2_queryctrl){
        .id = id | next_flags,
    };
    v4l2->ret = ioctl(v4l2->fd, VIDIOC_QUERYCTRL, args);
    v4l2_check(v4l2, "VIDIOC_QUERYCTRL");
}

static inline int
v4l2_vidioc_g_ctrl(struct v4l2 *v4l2, uint32_t id)
{
    struct v4l2_control args = {
        .id = id,
    };
    v4l2->ret = ioctl(v4l2->fd, VIDIOC_G_CTRL, &args);
    v4l2_check(v4l2, "VIDIOC_G_CTRL");

    return args.value;
}

static inline void
v4l2_init_device(struct v4l2 *v4l2)
{
    v4l2->fd = open(v4l2->params.path, O_RDWR);
    if (v4l2->fd < 0)
        v4l2_die("failed to open %s", v4l2->params.path);
}

static inline void
v4l2_init(struct v4l2 *v4l2, const struct v4l2_init_params *params)
{
    memset(v4l2, 0, sizeof(*v4l2));
    v4l2->params = *params;
    v4l2->fd = -1;

    v4l2_init_device(v4l2);
    v4l2_vidioc_querycap(v4l2, &v4l2->cap);
}

static inline void
v4l2_cleanup(struct v4l2 *v4l2)
{
    close(v4l2->fd);
}

static inline struct v4l2_queryctrl *
v4l2_enumerate_controls(struct v4l2 *v4l2, uint32_t *count)
{
    *count = v4l2_vidioc_queryctrl_count(v4l2);

    struct v4l2_queryctrl *ctrls = calloc(*count, sizeof(*ctrls));
    if (!ctrls)
        v4l2_die("failed to alloc ctrls");

    for (uint32_t i = 0; i < *count; i++) {
        const uint32_t prev_id = i > 0 ? ctrls[i - 1].id : 0;
        struct v4l2_queryctrl *ctrl = &ctrls[i];
        v4l2_vidioc_queryctrl_next(v4l2, prev_id, ctrl);
    }

    return ctrls;
}

static inline struct v4l2_fmtdesc *
v4l2_enumerate_formats(struct v4l2 *v4l2, enum v4l2_buf_type type, uint32_t *count)
{
    *count = v4l2_vidioc_enum_fmt_count(v4l2, type);

    struct v4l2_fmtdesc *descs = calloc(*count, sizeof(*descs));
    if (!descs)
        v4l2_die("failed to alloc fmtdescs");

    for (uint32_t i = 0; i < *count; i++) {
        struct v4l2_fmtdesc *desc = &descs[i];
        v4l2_vidioc_enum_fmt(v4l2, type, i, desc);
    }

    return descs;
}

static inline struct v4l2_frmsizeenum *
v4l2_enumerate_frame_sizes(struct v4l2 *v4l2, uint32_t format, uint32_t *count)
{
    *count = v4l2_vidioc_enum_framesizes_count(v4l2, format);

    struct v4l2_frmsizeenum *sizes = calloc(*count, sizeof(*sizes));
    if (!sizes)
        v4l2_die("failed to alloc frame sizes");

    for (uint32_t i = 0; i < *count; i++) {
        struct v4l2_frmsizeenum *size = &sizes[i];
        v4l2_vidioc_enum_framesizes(v4l2, format, i, size);
    }

    return sizes;
}

static inline struct v4l2_frmivalenum *
v4l2_enumerate_frame_intervals(
    struct v4l2 *v4l2, uint32_t width, uint32_t height, uint32_t format, uint32_t *count)
{
    *count = v4l2_vidioc_enum_frameintervals_count(v4l2, format, width, height);

    struct v4l2_frmivalenum *intervals = calloc(*count, sizeof(*intervals));
    if (!intervals)
        v4l2_die("failed to alloc frame intervals");

    for (uint32_t i = 0; i < *count; i++) {
        struct v4l2_frmivalenum *interval = &intervals[i];
        v4l2_vidioc_enum_frameintervals(v4l2, format, width, height, i, interval);
    }

    return intervals;
}

static inline struct v4l2_input *
v4l2_enumerate_inputs(struct v4l2 *v4l2, uint32_t *count)
{
    *count = v4l2_vidioc_enuminput_count(v4l2);

    struct v4l2_input *inputs = calloc(*count, sizeof(*inputs));
    if (!inputs)
        v4l2_die("failed to alloc inputs");

    for (uint32_t i = 0; i < *count; i++) {
        struct v4l2_input *input = &inputs[i];
        v4l2_vidioc_enuminput(v4l2, i, input);
    }

    return inputs;
}

/* FWIW, how capturing works is
 *
 *  - VIDIOC_REQBUFS to allcoate in-kernel buffers
 *  - VIDIOC_QUERYBUF to get magic offsets and mmap buffers to userspace
 *  - VIDIOC_QBUF to queue buffers
 *  - VIDIOC_STREAMON to start streaming
 *  - loop
 *    - VIDIOC_DQBUF to dequeue a buffer
 *    - save away the buffer data
 *    - VIDIOC_QBUF to queue the buffer back
 *  - VIDIOC_STREAMOFF to stop streaming
 *  - VIDIOC_REQBUFS to free buffers
 */

#endif /* V4L2UTIL_H */
