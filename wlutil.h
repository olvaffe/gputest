/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef WLUTIL_H
#define WLUTIL_H

#include "xdg-shell-protocol.h"

#include <linux/input.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#define PRINTFLIKE(f, a) __attribute__((format(printf, f, a)))
#define NORETURN __attribute__((noreturn))

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define ALIGN(v, a) (((v) + (a) - 1) & ~((a) - 1))

struct wl_init_params {
    void *data;
    void (*redraw)(void *data);
    void (*close)(void *data);
    void (*key)(void *data, uint32_t key);
};

struct wl {
    struct wl_init_params params;

    struct wl_display *display;
    int display_fd;

    struct wl_compositor *compositor;

    struct wl_seat *seat;
    struct wl_keyboard *keyboard;

    struct xdg_wm_base *wm_base;

    struct wl_shm *shm;
    struct wl_array shm_formats;

    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    bool xdg_ready;
};

struct wl_swapchain {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t image_count;

    uint32_t shm_size;

    struct wl_swapchain_image {
        struct wl_buffer *buffer;
        bool busy;
        void *data;
    } *images;
};

static inline void
wl_logv(const char *format, va_list ap)
{
    printf("VK: ");
    vprintf(format, ap);
    printf("\n");
}

static inline void NORETURN
wl_diev(const char *format, va_list ap)
{
    wl_logv(format, ap);
    abort();
}

static inline void PRINTFLIKE(1, 2) wl_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    wl_logv(format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(1, 2) NORETURN wl_die(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    wl_diev(format, ap);
    va_end(ap);
}

static void
xdg_toplevel_event_configure(void *data,
                             struct xdg_toplevel *toplevel,
                             int32_t width,
                             int32_t height,
                             struct wl_array *states)
{
}

static void
xdg_toplevel_event_close(void *data, struct xdg_toplevel *toplevel)
{
    struct wl *wl = data;

    if (wl->params.close)
        wl->params.close(wl->params.data);
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_event_configure,
    .close = xdg_toplevel_event_close,
};

static void
xdg_surface_event_configure(void *data, struct xdg_surface *surface, uint32_t serial)
{
    struct wl *wl = data;

    xdg_surface_ack_configure(surface, serial);
    wl->xdg_ready = true;

    if (wl->params.redraw)
        wl->params.redraw(wl->params.data);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_event_configure,
};

static void
wl_buffer_event_release(void *data, struct wl_buffer *buffer)
{
    struct wl_swapchain_image *img = data;
    img->busy = false;
}

static const struct wl_buffer_listener wl_buffer_listener = {
    .release = wl_buffer_event_release,
};

static void
wl_shm_event_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
    struct wl *wl = data;

    switch (format) {
    case WL_SHM_FORMAT_ARGB8888:
        format = DRM_FORMAT_ARGB8888;
        break;
    case WL_SHM_FORMAT_XRGB8888:
        format = DRM_FORMAT_XRGB8888;
        break;
    }

    uint32_t *iter = wl_array_add(&wl->shm_formats, sizeof(format));
    *iter = format;
}

static const struct wl_shm_listener wl_shm_listener = {
    .format = wl_shm_event_format,
};

static void
wl_keyboard_event_keymap(
    void *data, struct wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size)
{
    close(fd);
}

static void
wl_keyboard_event_enter(void *data,
                        struct wl_keyboard *wl_keyboard,
                        uint32_t serial,
                        struct wl_surface *surface,
                        struct wl_array *keys)
{
}

static void
wl_keyboard_event_leave(void *data,
                        struct wl_keyboard *wl_keyboard,
                        uint32_t serial,
                        struct wl_surface *surface)
{
}

static void
wl_keyboard_event_key(void *data,
                      struct wl_keyboard *wl_keyboard,
                      uint32_t serial,
                      uint32_t time,
                      uint32_t key,
                      uint32_t state)
{
    struct wl *wl = data;

    if (state == WL_KEYBOARD_KEY_STATE_RELEASED)
        wl->params.key(wl->params.data, key);
}

static void
wl_keyboard_event_modifiers(void *data,
                            struct wl_keyboard *wl_keyboard,
                            uint32_t serial,
                            uint32_t mods_depressed,
                            uint32_t mods_latched,
                            uint32_t mods_locked,
                            uint32_t group)
{
}

static void
wl_keyboard_event_repeat_info(void *data,
                              struct wl_keyboard *wl_keyboard,
                              int32_t rate,
                              int32_t delay)
{
}

static const struct wl_keyboard_listener wl_keyboard_listener = {
    .keymap = wl_keyboard_event_keymap,
    .enter = wl_keyboard_event_enter,
    .leave = wl_keyboard_event_leave,
    .key = wl_keyboard_event_key,
    .modifiers = wl_keyboard_event_modifiers,
    .repeat_info = wl_keyboard_event_repeat_info,
};

static void
wl_seat_event_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities)
{
    struct wl *wl = data;

    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !wl->keyboard) {
        wl->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(wl->keyboard, &wl_keyboard_listener, wl);
    } else if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && wl->keyboard) {
        wl_keyboard_destroy(wl->keyboard);
        wl->keyboard = NULL;
    }
}

static const struct wl_seat_listener wl_seat_listener = {
    .capabilities = wl_seat_event_capabilities,
};

static void
xdg_wm_base_event_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial)
{
    xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_event_ping,
};

static void
wl_registry_event_global(
    void *data, struct wl_registry *reg, uint32_t name, const char *interface, uint32_t version)
{
    struct wl *wl = data;

    if (!strcmp(interface, wl_compositor_interface.name)) {
        wl->compositor = wl_registry_bind(reg, name, &wl_compositor_interface,
                                          WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION);
    } else if (!strcmp(interface, xdg_wm_base_interface.name)) {
        wl->wm_base = wl_registry_bind(reg, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(wl->wm_base, &xdg_wm_base_listener, wl);
    } else if (!strcmp(interface, wl_seat_interface.name)) {
        wl->seat = wl_registry_bind(reg, name, &wl_seat_interface, 1);
        wl_seat_add_listener(wl->seat, &wl_seat_listener, wl);
    } else if (!strcmp(interface, wl_shm_interface.name)) {
        wl->shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
        wl_shm_add_listener(wl->shm, &wl_shm_listener, wl);

        wl_array_init(&wl->shm_formats);
    }
}

static void
wl_registry_event_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
}

static const struct wl_registry_listener wl_registry_listener = {
    .global = wl_registry_event_global,
    .global_remove = wl_registry_event_global_remove,
};

static inline void
wl_init_display(struct wl *wl)
{
    wl->display = wl_display_connect(NULL);
    if (!wl->display)
        wl_die("failed to connect to display");

    wl->display_fd = wl_display_get_fd(wl->display);
}

static inline void
wl_init_globals(struct wl *wl)
{
    struct wl_registry *reg = wl_display_get_registry(wl->display);
    wl_registry_add_listener(reg, &wl_registry_listener, wl);

    wl_display_roundtrip(wl->display);
    /* roundtrip again because we might have called wl_registry_bind */
    wl_display_roundtrip(wl->display);

    wl_registry_destroy(reg);
}

static inline void
wl_init_surface(struct wl *wl)
{
    wl->surface = wl_compositor_create_surface(wl->compositor);

    wl->xdg_surface = xdg_wm_base_get_xdg_surface(wl->wm_base, wl->surface);
    xdg_surface_add_listener(wl->xdg_surface, &xdg_surface_listener, wl);

    wl->xdg_toplevel = xdg_surface_get_toplevel(wl->xdg_surface);
    xdg_toplevel_add_listener(wl->xdg_toplevel, &xdg_toplevel_listener, wl);

    xdg_toplevel_set_title(wl->xdg_toplevel, "wlutil");

    wl_surface_commit(wl->surface);
}

static inline void
wl_init(struct wl *wl, const struct wl_init_params *params)
{
    memset(wl, 0, sizeof(*wl));
    if (params)
        wl->params = *params;

    wl_log_set_handler_client(wl_logv);

    wl_init_display(wl);
    wl_init_globals(wl);
    wl_init_surface(wl);
}

static inline void
wl_cleanup(struct wl *wl)
{
    xdg_toplevel_destroy(wl->xdg_toplevel);
    xdg_surface_destroy(wl->xdg_surface);
    wl_surface_destroy(wl->surface);

    wl_array_release(&wl->shm_formats);
    wl_shm_destroy(wl->shm);

    xdg_wm_base_destroy(wl->wm_base);

    wl_keyboard_destroy(wl->keyboard);
    wl_seat_destroy(wl->seat);

    wl_compositor_destroy(wl->compositor);

    wl_display_flush(wl->display);
    wl_display_disconnect(wl->display);
}

static inline void
wl_dispatch(struct wl *wl)
{
    if (wl_display_dispatch(wl->display) < 0)
        wl_die("failed to dispatch display");
}

static inline uint32_t
wl_drm_format_cpp(uint32_t format)
{
    switch (format) {
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XRGB8888:
        return 4;
    default:
        return 0;
    }
}

static inline uint32_t
wl_drm_format_to_shm_format(uint32_t format)
{
    switch (format) {
    case DRM_FORMAT_ARGB8888:
        return WL_SHM_FORMAT_ARGB8888;
    case DRM_FORMAT_XRGB8888:
        return WL_SHM_FORMAT_XRGB8888;
    default:
        return format;
    }
}

static inline struct wl_swapchain *
wl_create_swapchain(
    struct wl *wl, uint32_t width, uint32_t height, uint32_t format, uint32_t image_count)
{
    struct wl_swapchain *swapchain = calloc(1, sizeof(*swapchain));
    if (!swapchain)
        wl_die("failed to alloc swapchain");

    swapchain->images = calloc(image_count, sizeof(*swapchain->images));
    if (!swapchain->images)
        wl_die("failed to alloc swapchain images");

    if (!wl_drm_format_cpp(format))
        wl_die("unknown swapchain format");

    swapchain->width = width;
    swapchain->height = height;
    swapchain->format = format;
    swapchain->image_count = image_count;

    return swapchain;
}

static inline void
wl_destroy_swapchain(struct wl *wl, struct wl_swapchain *swapchain)
{
    for (uint32_t i = 0; i < swapchain->image_count; i++) {
        struct wl_swapchain_image *img = &swapchain->images[i];
        if (img->buffer)
            wl_buffer_destroy(img->buffer);
    }

    if (swapchain->shm_size)
        munmap(swapchain->images[0].data, swapchain->shm_size);

    free(swapchain->images);
    free(swapchain);
}

static inline void
wl_init_swapchain_images_shm(struct wl *wl, struct wl_swapchain *swapchain)
{
    const uint32_t *iter;
    bool found = false;
    wl_array_for_each(iter, &wl->shm_formats)
    {
        if (*iter == swapchain->format) {
            found = true;
            break;
        }
    }
    if (!found)
        wl_die("unsupported shm format '%.*s'", 4, (const char *)&swapchain->format);

    const uint32_t img_cpp = wl_drm_format_cpp(swapchain->format);
    const uint32_t img_pitch = img_cpp * swapchain->width;
    const uint32_t img_size = img_pitch * swapchain->height;

    const uint32_t shm_size = img_size * swapchain->image_count;
    const int shm_fd = memfd_create("swapchain", MFD_CLOEXEC);
    if (shm_fd < 0)
        wl_die("failed to create memfd");
    if (ftruncate(shm_fd, shm_size) < 0)
        wl_die("failed to truncate memfd");
    void *shm_ptr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED)
        wl_die("failed to map memfd");

    struct wl_shm_pool *shm_pool = wl_shm_create_pool(wl->shm, shm_fd, shm_size);

    for (uint32_t i = 0; i < swapchain->image_count; i++) {
        struct wl_swapchain_image *img = &swapchain->images[i];
        const uint32_t shm_offset = img_size * i;

        img->buffer =
            wl_shm_pool_create_buffer(shm_pool, shm_offset, swapchain->width, swapchain->height,
                                      img_pitch, wl_drm_format_to_shm_format(swapchain->format));
        wl_buffer_add_listener(img->buffer, &wl_buffer_listener, img);
        img->data = shm_ptr + shm_offset;
    }

    wl_shm_pool_destroy(shm_pool);
    close(shm_fd);

    swapchain->shm_size = shm_size;
}

static inline const struct wl_swapchain_image *
wl_acquire_swapchain_image(struct wl *wl, struct wl_swapchain *swapchain)
{
    for (uint32_t i = 0; i < swapchain->image_count; i++) {
        struct wl_swapchain_image *img = &swapchain->images[i];
        if (!img->busy) {
            img->busy = true;
            return img;
        }
    }

    vk_die("no idle swapchain image");
}

static inline void
wl_present_swapchain_image(struct wl *wl,
                           struct wl_swapchain *swapchain,
                           const struct wl_swapchain_image *img)
{
    assert(img >= swapchain->images && img - swapchain->images < swapchain->image_count);
    assert(wl->xdg_ready);

    wl_surface_attach(wl->surface, img->buffer, 0, 0);
    wl_surface_damage_buffer(wl->surface, 0, 0, swapchain->width, swapchain->height);
    wl_surface_commit(wl->surface);
}

#endif /* WLUTIL_H */
