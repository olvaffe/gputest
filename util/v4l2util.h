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

    struct v4l2_queryctrl *ctrls;
    uint32_t ctrl_count;
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
v4l2_init_device(struct v4l2 *v4l2)
{
    v4l2->fd = open(v4l2->params.path, O_RDWR);
    if (v4l2->fd < 0)
        v4l2_die("failed to open %s", v4l2->params.path);

    v4l2->ret = ioctl(v4l2->fd, VIDIOC_QUERYCAP, &v4l2->cap);
    v4l2_check(v4l2, "failed to VIDIOC_QUERYCAP");
}

static inline void
v4l2_init_controls(struct v4l2 *v4l2)
{
    struct v4l2_queryctrl query;

    query.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    while (true) {
        if (ioctl(v4l2->fd, VIDIOC_QUERYCTRL, &query) < 0)
            break;
        v4l2->ctrl_count++;
        query.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }

    v4l2->ctrls = calloc(v4l2->ctrl_count, sizeof(*v4l2->ctrls));
    if (!v4l2->ctrls)
        v4l2_die("failed to alloc ctrls");

    v4l2->ctrl_count = 0;
    query.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    while (true) {
        if (ioctl(v4l2->fd, VIDIOC_QUERYCTRL, &query) < 0)
            break;
        v4l2->ctrls[v4l2->ctrl_count++] = query;
        query.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }
}

static inline void
v4l2_init(struct v4l2 *v4l2, const struct v4l2_init_params *params)
{
    memset(v4l2, 0, sizeof(*v4l2));
    v4l2->params = *params;
    v4l2->fd = -1;

    v4l2_init_device(v4l2);
    v4l2_init_controls(v4l2);
}

static inline void
v4l2_cleanup(struct v4l2 *v4l2)
{
    free(v4l2->ctrls);
    close(v4l2->fd);
}

static inline int
v4l2_get_ctrl(struct v4l2 *v4l2, uint32_t ctrl_id)
{
    struct v4l2_control data = {
        .id = ctrl_id,
    };
    v4l2->ret = ioctl(v4l2->fd, VIDIOC_G_CTRL, &data);
    v4l2_check(v4l2, "VIDIOC_G_CTRL");

    return data.value;
}

static inline struct v4l2_fmtdesc *
v4l2_enumerate_formats(struct v4l2 *v4l2, enum v4l2_buf_type type, uint32_t *count)
{
    *count = 0;
    while (true) {
        struct v4l2_fmtdesc desc = {
            .index = *count,
            .type = type,
        };
        if (ioctl(v4l2->fd, VIDIOC_ENUM_FMT, &desc))
            break;
        (*count)++;
    }

    struct v4l2_fmtdesc *descs = calloc(*count, sizeof(*descs));
    if (!descs)
        v4l2_die("failed to alloc fmtdescs");

    for (uint32_t i = 0; i < *count; i++) {
        struct v4l2_fmtdesc *desc = &descs[i];
        desc->index = i;
        desc->type = type;

        v4l2->ret = ioctl(v4l2->fd, VIDIOC_ENUM_FMT, desc);
        v4l2_check(v4l2, "VIDIOC_ENUM_FMT");
    }

    return descs;
}

static inline struct v4l2_frmsizeenum *
v4l2_enumerate_frame_sizes(struct v4l2 *v4l2, uint32_t format, uint32_t *count)
{
    *count = 0;
    while (true) {
        struct v4l2_frmsizeenum size = {
            .index = *count,
            .pixel_format = format,
        };
        if (ioctl(v4l2->fd, VIDIOC_ENUM_FRAMESIZES, &size))
            break;
        (*count)++;
    }

    struct v4l2_frmsizeenum *sizes = calloc(*count, sizeof(*sizes));
    if (!sizes)
        v4l2_die("failed to alloc frame sizes");

    for (uint32_t i = 0; i < *count; i++) {
        struct v4l2_frmsizeenum *size = &sizes[i];
        size->index = i;
        size->pixel_format = format;
        v4l2->ret = ioctl(v4l2->fd, VIDIOC_ENUM_FRAMESIZES, size);
        v4l2_check(v4l2, "VIDIOC_ENUM_FRAMESIZES");
    }

    return sizes;
}

static inline struct v4l2_frmivalenum *
v4l2_enumerate_frame_intervals(
    struct v4l2 *v4l2, uint32_t width, uint32_t height, uint32_t format, uint32_t *count)
{
    *count = 0;
    while (true) {
        struct v4l2_frmivalenum data = {
            .index = *count,
            .pixel_format = format,
            .width = width,
            .height = height,
        };
        if (ioctl(v4l2->fd, VIDIOC_ENUM_FRAMEINTERVALS, &data))
            break;
        (*count)++;
    }

    struct v4l2_frmivalenum *intervals = calloc(*count, sizeof(*intervals));
    if (!intervals)
        v4l2_die("failed to alloc frame intervals");

    for (uint32_t i = 0; i < *count; i++) {
        struct v4l2_frmivalenum *interval = &intervals[i];
        interval->index = i;
        interval->pixel_format = format;
        interval->width = width;
        interval->height = height;
        v4l2->ret = ioctl(v4l2->fd, VIDIOC_ENUM_FRAMEINTERVALS, interval);
        v4l2_check(v4l2, "VIDIOC_ENUM_FRAMEINTERVALS");
    }

    return intervals;
}

static inline struct v4l2_input *
v4l2_enumerate_inputs(struct v4l2 *v4l2, uint32_t *count)
{
    *count = 0;
    while (true) {
        struct v4l2_input input = {
            .index = *count,
        };
        if (ioctl(v4l2->fd, VIDIOC_ENUMINPUT, &input))
            break;
        (*count)++;
    }

    struct v4l2_input *inputs = calloc(*count, sizeof(*inputs));
    if (!inputs)
        v4l2_die("failed to alloc inputs");

    for (uint32_t i = 0; i < *count; i++) {
        struct v4l2_input *input = &inputs[i];
        input->index = i;

        v4l2->ret = ioctl(v4l2->fd, VIDIOC_ENUMINPUT, input);
        v4l2_check(v4l2, "VIDIOC_ENUMINPUT");
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
