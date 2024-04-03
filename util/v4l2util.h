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

#endif /* V4L2UTIL_H */
