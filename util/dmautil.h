/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef DMAUTIL_H
#define DMAUTIL_H

#include "util.h"

#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <sys/ioctl.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct dma_buf {
    int fd;
    size_t size;

    void *map;
    uint64_t sync_flags;
};

struct dma_heap {
    int fd;
};

static inline void PRINTFLIKE(1, 2) dma_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_logv("DMA", format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(1, 2) NORETURN dma_die(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_diev("DMA", format, ap);
    va_end(ap);
}

static inline void
dma_buf_sync(int fd, uint64_t flags)
{
    struct dma_buf_sync args = {
        .flags = flags,
    };

    if (ioctl(fd, DMA_BUF_IOCTL_SYNC, &args))
        dma_die("failed to sync dma-buf");
}

static inline struct dma_buf *
dma_buf_create(int fd)
{
    struct dma_buf *buf = (struct dma_buf *)calloc(1, sizeof(*buf));
    if (!buf)
        dma_die("failed to alloc dma_buf");

    /* take ownership */
    buf->fd = fd;

    const off_t off = lseek(buf->fd, 0, SEEK_END);
    if (off < 0)
        dma_die("failed to seek dma-buf");

    buf->size = off;

    return buf;
}

static inline void
dma_buf_destroy(struct dma_buf *buf)
{
    close(buf->fd);
    free(buf);
}

static inline void *
dma_buf_map(struct dma_buf *buf)
{
    if (buf->map)
        dma_die("nested dma-buf mmap");

    buf->map = mmap(NULL, buf->size, PROT_READ | PROT_WRITE, MAP_SHARED, buf->fd, 0);
    if (buf->map == MAP_FAILED)
        dma_die("failed to mmap dma-buf");

    return buf->map;
}

static inline void
dma_buf_unmap(struct dma_buf *buf)
{
    munmap(buf->map, buf->size);
}

static inline void
dma_buf_start(struct dma_buf *buf, uint64_t flags)
{
    dma_buf_sync(buf->fd, DMA_BUF_SYNC_START | flags);
    buf->sync_flags = flags;
}

static inline void
dma_buf_end(struct dma_buf *buf)
{
    dma_buf_sync(buf->fd, DMA_BUF_SYNC_END | buf->sync_flags);
    buf->sync_flags = 0;
}

static inline void
dma_heap_init(struct dma_heap *heap, const char *heap_name)
{
    memset(heap, 0, sizeof(*heap));
    heap->fd = -1;

    char heap_path[1024];
    snprintf(heap_path, sizeof(heap_path), "/dev/dma_heap/%s", heap_name);

    heap->fd = open(heap_path, O_RDONLY);
    if (heap->fd < 0)
        dma_die("failed to open %s", heap_path);
}

static inline void
dma_heap_cleanup(struct dma_heap *heap)
{
    if (heap->fd >= 0)
        close(heap->fd);
}

static inline struct dma_buf *
dma_heap_alloc(struct dma_heap *heap, size_t size)
{
    struct dma_heap_allocation_data args = {
        .len = size,
        .fd_flags = O_RDWR | O_CLOEXEC,
    };

    if (ioctl(heap->fd, DMA_HEAP_IOCTL_ALLOC, &args))
        dma_die("failed to alloc dma-buf");

    return dma_buf_create(args.fd);
}

#if defined(__cplusplus)
}
#endif

#endif /* DMAUTIL_H */
