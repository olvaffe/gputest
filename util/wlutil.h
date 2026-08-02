/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef WLUTIL_H
#define WLUTIL_H

#include "color-management-v1-client-protocol.h"
#include "commit-timing-v1-client-protocol.h"
#include "fifo-v1-client-protocol.h"
#include "linux-dmabuf-v1-client-protocol.h"
#include "presentation-time-client-protocol.h"
#include "tearing-control-v1-client-protocol.h"
#include "util.h"
#include "xdg-shell-client-protocol.h"

#include <linux/input-event-codes.h>
#include <wayland-client.h>

#define wl_die(format, ...) u_die("WL", format __VA_OPT__(, ) __VA_ARGS__)
#define wl_log(format, ...) u_log("WL", format __VA_OPT__(, ) __VA_ARGS__)

struct wl_init_params {
    void *data;
    void (*redraw)(void *data);
    void (*close)(void *data);
    void (*key)(void *data, uint32_t key);
};

struct wl_output_info {
    struct wl_output *output;

    char *make;
    char *model;

    int32_t width;
    int32_t height;
    int32_t refresh_rate;

    int32_t scale;

    struct wp_color_management_output_v1 *cm_output;
    struct wp_image_description_v1 *cm_desc;

    enum wp_color_manager_v1_primaries cm_primaries;
    enum wp_color_manager_v1_transfer_function cm_tf;

    struct wl_list node;
};

struct wl {
    struct wl_init_params params;

    struct wl_display *display;
    int display_fd;

    struct wl_list outputs;

    struct wp_color_manager_v1 *color_manager;

    struct wl_seat *seat;
    struct wl_keyboard *keyboard;

    struct wl_compositor *compositor;
    uint32_t compositor_version;

    struct wp_presentation *presentation;
    uint32_t presentation_clock_id;

    struct wp_commit_timing_manager_v1 *commit_timing_manager;

    struct wp_fifo_manager_v1 *fifo_manager;

    struct wp_tearing_control_manager_v1 *tearing_control_manager;

    struct xdg_wm_base *wm_base;

    struct wl_shm *shm;
    struct wl_array shm_formats;

    struct zwp_linux_dmabuf_v1 *dmabuf;
    uint32_t dmabuf_version;

    struct wl_surface *surface;
    struct wp_commit_timer_v1 *commit_timer;
    struct wp_fifo_v1 *fifo;
    struct wp_color_management_surface_v1 *cm_surface;
    struct wp_image_description_v1 *cm_desc;
    struct wp_tearing_control_v1 *tearing_control;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    bool xdg_ready;

    struct zwp_linux_dmabuf_feedback_v1 *dmabuf_feedback;
    const void *dmabuf_format_table;
    uint32_t dmabuf_format_table_size;
    struct {
        dev_t main_dev;
        dev_t target_dev;
        bool scanout;
        struct wl_array formats;
        uint32_t tranche_count;
    } pending, active;

    bool dispatch_ready;
};

struct wl_swapchain {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint64_t modifier;
    uint32_t image_count;

    uint32_t shm_size;

    struct wl_swapchain_image {
        struct wl_buffer *buffer;
        bool busy;
        void *data;
    } *images;
};

static void
wp_presentation_feedback_event_sync_output(void *data,
                                           struct wp_presentation_feedback *feedback,
                                           struct wl_output *output)
{
}

static void
wp_presentation_feedback_event_presented(void *data,
                                         struct wp_presentation_feedback *feedback,
                                         uint32_t tv_sec_hi,
                                         uint32_t tv_sec_lo,
                                         uint32_t tv_nsec,
                                         uint32_t refresh,
                                         uint32_t seq_hi,
                                         uint32_t seq_lo,
                                         uint32_t flags)
{
    const uint64_t sec = ((uint64_t)tv_sec_hi << 32) | tv_sec_lo;
    const uint64_t seq = ((uint64_t)seq_hi << 32) | seq_lo;
    wl_log("presented sec %" PRIu64 " nsec %u refresh %u seq %" PRIu64 " flags 0x%x", sec,
           tv_nsec, refresh, seq, flags);

    wp_presentation_feedback_destroy(feedback);
}

static void
wp_presentation_feedback_event_discarded(void *data, struct wp_presentation_feedback *feedback)
{
    wl_log("presentation feedback discarded");
    wp_presentation_feedback_destroy(feedback);
}

static const struct wp_presentation_feedback_listener wp_presentation_feedback_listener = {
    .sync_output = wp_presentation_feedback_event_sync_output,
    .presented = wp_presentation_feedback_event_presented,
    .discarded = wp_presentation_feedback_event_discarded,
};

static void
zwp_linux_dmabuf_feedback_v1_event_format_table(void *data,
                                                struct zwp_linux_dmabuf_feedback_v1 *feedback,
                                                int32_t fd,
                                                uint32_t size)
{
    struct wl *wl = data;

    if (wl->dmabuf_format_table)
        munmap((void *)wl->dmabuf_format_table, wl->dmabuf_format_table_size);

    wl->dmabuf_format_table = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (wl->dmabuf_format_table == MAP_FAILED)
        wl_die("failed to map format table");
    close(fd);

    wl->dmabuf_format_table_size = size;
}

static void
zwp_linux_dmabuf_feedback_v1_event_main_device(void *data,
                                               struct zwp_linux_dmabuf_feedback_v1 *feedback,
                                               struct wl_array *dev)
{
    struct wl *wl = data;

    if (dev->size != sizeof(wl->pending.main_dev))
        wl_die("bad main dev size");
    memcpy(&wl->pending.main_dev, dev->data, dev->size);

    wl->pending.tranche_count = 0;
}

static void
zwp_linux_dmabuf_feedback_v1_event_tranche_target_device(
    void *data, struct zwp_linux_dmabuf_feedback_v1 *feedback, struct wl_array *dev)
{
    struct wl *wl = data;

    if (wl->pending.tranche_count)
        return;

    if (dev->size != sizeof(wl->pending.target_dev))
        wl_die("bad target dev size");
    memcpy(&wl->pending.target_dev, dev->data, dev->size);
}

static void
zwp_linux_dmabuf_feedback_v1_event_tranche_flags(void *data,
                                                 struct zwp_linux_dmabuf_feedback_v1 *feedback,
                                                 uint32_t flags)
{
    struct wl *wl = data;

    if (wl->pending.tranche_count)
        return;

    if (flags & ZWP_LINUX_DMABUF_FEEDBACK_V1_TRANCHE_FLAGS_SCANOUT)
        wl->pending.scanout = true;
}

static void
zwp_linux_dmabuf_feedback_v1_event_tranche_formats(void *data,
                                                   struct zwp_linux_dmabuf_feedback_v1 *feedback,
                                                   struct wl_array *indices)
{
    struct wl *wl = data;

    if (wl->pending.tranche_count)
        return;

    wl_array_init(&wl->pending.formats);

    const uint16_t *idx_iter;
    wl_array_for_each(idx_iter, indices) {
        const uint32_t offset = *idx_iter * 16;
        const uint32_t *fmt = wl->dmabuf_format_table + offset;
        const uint64_t *mod = wl->dmabuf_format_table + offset + 8;

        struct wl_array *fmt_iter;
        bool found = false;
        wl_array_for_each(fmt_iter, &wl->pending.formats) {
            const uint64_t *fmt_iter_fmt = fmt_iter->data;
            if (*fmt_iter_fmt == *fmt) {
                found = true;
                break;
            }
        }
        if (!found) {
            fmt_iter = wl_array_add(&wl->pending.formats, sizeof(*fmt_iter));
            wl_array_init(fmt_iter);

            uint64_t *fmt_iter_fmt = wl_array_add(fmt_iter, sizeof(*fmt_iter_fmt));
            *fmt_iter_fmt = *fmt;
        }

        uint64_t *mod_iter = wl_array_add(fmt_iter, sizeof(*mod_iter));
        *mod_iter = *mod;
    }
}

static void
zwp_linux_dmabuf_feedback_v1_event_tranche_done(void *data,
                                                struct zwp_linux_dmabuf_feedback_v1 *feedback)
{
    struct wl *wl = data;
    wl->pending.tranche_count++;
}

static void
zwp_linux_dmabuf_feedback_v1_event_done(void *data, struct zwp_linux_dmabuf_feedback_v1 *feedback)
{
    struct wl *wl = data;

    struct wl_array *dmabuf_iter;
    wl_array_for_each(dmabuf_iter, &wl->active.formats)
        wl_array_release(dmabuf_iter);
    wl_array_release(&wl->active.formats);

    wl->active = wl->pending;
}

static const struct zwp_linux_dmabuf_feedback_v1_listener zwp_linux_dmabuf_feedback_v1_listener = {
    .format_table = zwp_linux_dmabuf_feedback_v1_event_format_table,
    .main_device = zwp_linux_dmabuf_feedback_v1_event_main_device,
    .tranche_target_device = zwp_linux_dmabuf_feedback_v1_event_tranche_target_device,
    .tranche_formats = zwp_linux_dmabuf_feedback_v1_event_tranche_formats,
    .tranche_flags = zwp_linux_dmabuf_feedback_v1_event_tranche_flags,
    .tranche_done = zwp_linux_dmabuf_feedback_v1_event_tranche_done,
    .done = zwp_linux_dmabuf_feedback_v1_event_done,
};

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

    if (wl->dispatch_ready && wl->params.close)
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

    if (wl->dispatch_ready && wl->params.redraw)
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
zwp_linux_dmabuf_v1_event_format_legacy(void *data,
                                        struct zwp_linux_dmabuf_v1 *dmabuf,
                                        uint32_t format)
{
}

static void
zwp_linux_dmabuf_v1_event_modifier_legacy(void *data,
                                          struct zwp_linux_dmabuf_v1 *dmabuf,
                                          uint32_t format,
                                          uint32_t modifier_hi,
                                          uint32_t modifier_lo)
{
    struct wl *wl = data;

    assert(wl->dmabuf_version < ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION);

    struct wl_array *fmt_iter;
    bool found = false;
    wl_array_for_each(fmt_iter, &wl->active.formats) {
        const uint64_t *fmt_iter_fmt = fmt_iter->data;
        if (*fmt_iter_fmt == format) {
            found = true;
            break;
        }
    }
    if (!found) {
        fmt_iter = wl_array_add(&wl->active.formats, sizeof(*fmt_iter));
        wl_array_init(fmt_iter);

        uint64_t *fmt_iter_fmt = wl_array_add(fmt_iter, sizeof(*fmt_iter_fmt));
        *fmt_iter_fmt = format;
    }

    uint64_t *mod_iter = wl_array_add(fmt_iter, sizeof(*mod_iter));
    *mod_iter = (uint64_t)modifier_hi << 32 | modifier_lo;
}

static const struct zwp_linux_dmabuf_v1_listener zwp_linux_dmabuf_v1_listener_legacy = {
    .format = zwp_linux_dmabuf_v1_event_format_legacy,
    .modifier = zwp_linux_dmabuf_v1_event_modifier_legacy,
};

static void
wl_shm_event_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
    struct wl *wl = data;

    uint32_t *iter = wl_array_add(&wl->shm_formats, sizeof(format));
    *iter = format;
}

static const struct wl_shm_listener wl_shm_listener = {
    .format = wl_shm_event_format,
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
wp_presentation_event_clock_id(void *data, struct wp_presentation *presentation, uint32_t clk_id)
{
    struct wl *wl = data;
    wl->presentation_clock_id = clk_id;
}

static const struct wp_presentation_listener wp_presentation_listener = {
    .clock_id = wp_presentation_event_clock_id,
};

static void
wl_image_description_info_event_done(void *data, struct wp_image_description_info_v1 *info)
{
}

static void
wl_image_description_info_event_icc_file(void *data,
                                         struct wp_image_description_info_v1 *info,
                                         int32_t icc,
                                         uint32_t icc_size)
{
    close(icc);
}

static void
wl_image_description_info_event_primaries(void *data,
                                          struct wp_image_description_info_v1 *info,
                                          int32_t r_x,
                                          int32_t r_y,
                                          int32_t g_x,
                                          int32_t g_y,
                                          int32_t b_x,
                                          int32_t b_y,
                                          int32_t w_x,
                                          int32_t w_y)
{
}

static void
wl_image_description_info_event_primaries_named(void *data,
                                                struct wp_image_description_info_v1 *info,
                                                uint32_t primaries)
{
    struct wl_output_info *out = data;
    out->cm_primaries = primaries;
}

static void
wl_image_description_info_event_tf_power(void *data,
                                         struct wp_image_description_info_v1 *info,
                                         uint32_t eexp)
{
}

static void
wl_image_description_info_event_tf_named(void *data,
                                         struct wp_image_description_info_v1 *info,
                                         uint32_t tf)
{
    struct wl_output_info *out = data;
    out->cm_tf = tf;
}

static void
wl_image_description_info_event_luminances(void *data,
                                           struct wp_image_description_info_v1 *info,
                                           uint32_t min_lum,
                                           uint32_t max_lum,
                                           uint32_t reference_lum)
{
}

static void
wl_image_description_info_event_target_primaries(void *data,
                                                 struct wp_image_description_info_v1 *info,
                                                 int32_t r_x,
                                                 int32_t r_y,
                                                 int32_t g_x,
                                                 int32_t g_y,
                                                 int32_t b_x,
                                                 int32_t b_y,
                                                 int32_t w_x,
                                                 int32_t w_y)
{
}

static void
wl_image_description_info_event_target_luminance(void *data,
                                                 struct wp_image_description_info_v1 *info,
                                                 uint32_t min_lum,
                                                 uint32_t max_lum)
{
}

static void
wl_image_description_info_event_target_max_cll(void *data,
                                               struct wp_image_description_info_v1 *info,
                                               uint32_t max_cll)
{
}

static void
wl_image_description_info_event_target_max_fall(void *data,
                                                struct wp_image_description_info_v1 *info,
                                                uint32_t max_fall)
{
}

static const struct wp_image_description_info_v1_listener wl_image_description_info_listener = {
    .done = wl_image_description_info_event_done,
    .icc_file = wl_image_description_info_event_icc_file,
    .primaries = wl_image_description_info_event_primaries,
    .primaries_named = wl_image_description_info_event_primaries_named,
    .tf_power = wl_image_description_info_event_tf_power,
    .tf_named = wl_image_description_info_event_tf_named,
    .luminances = wl_image_description_info_event_luminances,
    .target_primaries = wl_image_description_info_event_target_primaries,
    .target_luminance = wl_image_description_info_event_target_luminance,
    .target_max_cll = wl_image_description_info_event_target_max_cll,
    .target_max_fall = wl_image_description_info_event_target_max_fall,
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

    if (state == WL_KEYBOARD_KEY_STATE_RELEASED && wl->dispatch_ready && wl->params.key)
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
wl_output_event_geometry(void *data,
                         struct wl_output *output,
                         int32_t x,
                         int32_t y,
                         int32_t physical_width,
                         int32_t physical_height,
                         int32_t subpixel,
                         const char *make,
                         const char *model,
                         int32_t transform)
{
    struct wl_output_info *out = data;

    free(out->make);
    free(out->model);
    out->make = strdup(make);
    out->model = strdup(model);
}

static void
wl_output_event_mode(void *data,
                     struct wl_output *output,
                     uint32_t flags,
                     int32_t width,
                     int32_t height,
                     int32_t refresh)
{
    struct wl_output_info *out = data;

    if (!(flags & WL_OUTPUT_MODE_CURRENT))
        return;

    out->width = width;
    out->height = height;
    out->refresh_rate = refresh;
}

static void
wl_output_event_done(void *data, struct wl_output *output)
{
}

static void
wl_output_event_scale(void *data, struct wl_output *output, int32_t factor)
{
    struct wl_output_info *out = data;

    out->scale = factor;
}

static const struct wl_output_listener wl_output_listener = {
    .geometry = wl_output_event_geometry,
    .mode = wl_output_event_mode,
    .done = wl_output_event_done,
    .scale = wl_output_event_scale,
};

static void
wl_registry_event_global(
    void *data, struct wl_registry *reg, uint32_t name, const char *interface, uint32_t version)
{
    struct wl *wl = data;

    if (!strcmp(interface, wl_output_interface.name)) {
        if (version < WL_OUTPUT_RELEASE_SINCE_VERSION) {
            wl_die("%s ver %d req %d", interface, version, WL_OUTPUT_RELEASE_SINCE_VERSION);
        }
        version = WL_OUTPUT_RELEASE_SINCE_VERSION;

        struct wl_output_info *out = calloc(1, sizeof(*out));
        if (!out)
            wl_die("failed to alloc output info");
        out->scale = 1;

        out->output = wl_registry_bind(reg, name, &wl_output_interface, version);
        wl_output_add_listener(out->output, &wl_output_listener, out);
        wl_list_insert(&wl->outputs, &out->node);
    } else if (!strcmp(interface, wp_color_manager_v1_interface.name)) {
        wl->color_manager = wl_registry_bind(reg, name, &wp_color_manager_v1_interface, 1);
    } else if (!strcmp(interface, wl_seat_interface.name)) {
        wl->seat = wl_registry_bind(reg, name, &wl_seat_interface, 1);
        wl_seat_add_listener(wl->seat, &wl_seat_listener, wl);
    } else if (!strcmp(interface, wl_compositor_interface.name)) {
        if (version < WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION) {
            wl_die("%s ver %d req %d", interface, version,
                   WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION);
        }
        version = WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION;

        wl->compositor = wl_registry_bind(reg, name, &wl_compositor_interface, version);
        wl->compositor_version = version;
    } else if (!strcmp(interface, wp_presentation_interface.name)) {
        wl->presentation = wl_registry_bind(reg, name, &wp_presentation_interface, 1);
        wp_presentation_add_listener(wl->presentation, &wp_presentation_listener, wl);
    } else if (!strcmp(interface, wp_commit_timing_manager_v1_interface.name)) {
        wl->commit_timing_manager =
            wl_registry_bind(reg, name, &wp_commit_timing_manager_v1_interface, 1);
    } else if (!strcmp(interface, wp_fifo_manager_v1_interface.name)) {
        wl->fifo_manager = wl_registry_bind(reg, name, &wp_fifo_manager_v1_interface, 1);
    } else if (!strcmp(interface, wp_tearing_control_manager_v1_interface.name)) {
        wl->tearing_control_manager =
            wl_registry_bind(reg, name, &wp_tearing_control_manager_v1_interface, 1);
    } else if (!strcmp(interface, xdg_wm_base_interface.name)) {
        wl->wm_base = wl_registry_bind(reg, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(wl->wm_base, &xdg_wm_base_listener, wl);
    } else if (!strcmp(interface, wl_shm_interface.name)) {
        wl->shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
        wl_shm_add_listener(wl->shm, &wl_shm_listener, wl);

        wl_array_init(&wl->shm_formats);
    } else if (!strcmp(interface, zwp_linux_dmabuf_v1_interface.name)) {
        if (version < ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION) {
            wl_die("%s ver %d req %d", interface, version,
                   ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION);
        }
        if (version >= ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION)
            version = ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION;

        wl->dmabuf = wl_registry_bind(reg, name, &zwp_linux_dmabuf_v1_interface, version);
        wl->dmabuf_version = version;
        if (version < ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION) {
            zwp_linux_dmabuf_v1_add_listener(wl->dmabuf, &zwp_linux_dmabuf_v1_listener_legacy,
                                             wl);
            wl_array_init(&wl->active.formats);
            wl->active.tranche_count = 1;
        }
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
        wl_die("failed to connect to display: no or bad WAYLAND_DISPLAY?");

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

    if (!wl->compositor)
        wl_die("missing required global: wl_compositor");
    if (!wl->wm_base)
        wl_die("missing required global: xdg_wm_base");
    if (!wl->shm)
        wl_die("missing required global: wl_shm");
}

static inline void
wl_init_outputs(struct wl *wl)
{
    if (wl->color_manager) {
        struct wl_output_info *out;
        wl_list_for_each(out, &wl->outputs, node) {
            out->cm_output = wp_color_manager_v1_get_output(wl->color_manager, out->output);
            out->cm_desc = wp_color_management_output_v1_get_image_description(out->cm_output);

            struct wp_image_description_info_v1 *info =
                wp_image_description_v1_get_information(out->cm_desc);
            wp_image_description_info_v1_add_listener(info, &wl_image_description_info_listener,
                                                      out);
            wl_display_roundtrip(wl->display);

            wp_image_description_info_v1_destroy(info);
        }
    }
}

static inline void
wl_init_surface_dmabuf(struct wl *wl)
{
    if (wl->dmabuf_version < ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION)
        return;

    wl->dmabuf_feedback = zwp_linux_dmabuf_v1_get_surface_feedback(wl->dmabuf, wl->surface);
    zwp_linux_dmabuf_feedback_v1_add_listener(wl->dmabuf_feedback,
                                              &zwp_linux_dmabuf_feedback_v1_listener, wl);
    wl_display_roundtrip(wl->display);
}

static inline void
wl_init_surface_xdg(struct wl *wl)
{
    wl->xdg_surface = xdg_wm_base_get_xdg_surface(wl->wm_base, wl->surface);
    xdg_surface_add_listener(wl->xdg_surface, &xdg_surface_listener, wl);

    wl->xdg_toplevel = xdg_surface_get_toplevel(wl->xdg_surface);
    xdg_toplevel_add_listener(wl->xdg_toplevel, &xdg_toplevel_listener, wl);

    xdg_toplevel_set_title(wl->xdg_toplevel, "wlutil");
}

static inline void
wl_init_surface_cm(struct wl *wl)
{
    const enum wp_color_manager_v1_primaries primaries = WP_COLOR_MANAGER_V1_PRIMARIES_SRGB;
    const enum wp_color_manager_v1_transfer_function tf =
        WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22;
    const enum wp_color_manager_v1_render_intent render_intent =
        WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL;

    if (!wl->color_manager)
        return;

    wl->cm_surface = wp_color_manager_v1_get_surface(wl->color_manager, wl->surface);

    struct wp_image_description_creator_params_v1 *creator =
        wp_color_manager_v1_create_parametric_creator(wl->color_manager);
    wp_image_description_creator_params_v1_set_primaries_named(creator, primaries);
    wp_image_description_creator_params_v1_set_tf_named(creator, tf);
    wl->cm_desc = wp_image_description_creator_params_v1_create(creator);

    wp_color_management_surface_v1_set_image_description(wl->cm_surface, wl->cm_desc,
                                                         render_intent);
}

static inline void
wl_init_surface_tearing(struct wl *wl)
{
    if (!wl->tearing_control_manager)
        return;

    wl->tearing_control = wp_tearing_control_manager_v1_get_tearing_control(
        wl->tearing_control_manager, wl->surface);
    wp_tearing_control_v1_set_presentation_hint(wl->tearing_control,
                                                WP_TEARING_CONTROL_V1_PRESENTATION_HINT_VSYNC);
}

static inline void
wl_init_surface_fifo(struct wl *wl)
{
    if (!wl->fifo_manager)
        return;

    wl->fifo = wp_fifo_manager_v1_get_fifo(wl->fifo_manager, wl->surface);
}

static inline void
wl_init_surface_commit_timing(struct wl *wl)
{
    if (!wl->commit_timing_manager)
        return;

    wl->commit_timer =
        wp_commit_timing_manager_v1_get_timer(wl->commit_timing_manager, wl->surface);
}

static inline void
wl_init_surface(struct wl *wl)
{
    wl->surface = wl_compositor_create_surface(wl->compositor);

    wl_init_surface_commit_timing(wl);
    wl_init_surface_fifo(wl);
    wl_init_surface_tearing(wl);
    wl_init_surface_cm(wl);
    wl_init_surface_xdg(wl);
    wl_init_surface_dmabuf(wl);

    wl_surface_commit(wl->surface);
}

static inline void
wl_log_handler(const char *format, va_list ap)
{
    u_logv("WL", format, ap);
}

static inline void
wl_init(struct wl *wl, const struct wl_init_params *params)
{
    memset(wl, 0, sizeof(*wl));
    wl_list_init(&wl->outputs);

    if (params)
        wl->params = *params;

    wl_log_set_handler_client(wl_log_handler);

    wl_init_display(wl);
    wl_init_globals(wl);
    wl_init_outputs(wl);
    wl_init_surface(wl);

    wl_display_roundtrip(wl->display);
    wl->dispatch_ready = true;
}

static inline void
wl_cleanup(struct wl *wl)
{
    struct wl_array *dmabuf_iter;
    wl_array_for_each(dmabuf_iter, &wl->active.formats)
        wl_array_release(dmabuf_iter);
    wl_array_release(&wl->active.formats);

    if (wl->dmabuf_format_table)
        munmap((void *)wl->dmabuf_format_table, wl->dmabuf_format_table_size);

    if (wl->dmabuf_feedback)
        zwp_linux_dmabuf_feedback_v1_destroy(wl->dmabuf_feedback);

    if (wl->tearing_control_manager) {
        wp_tearing_control_v1_destroy(wl->tearing_control);
        wp_tearing_control_manager_v1_destroy(wl->tearing_control_manager);
    }

    if (wl->cm_surface) {
        wp_color_management_surface_v1_destroy(wl->cm_surface);
        if (wl->cm_desc)
            wp_image_description_v1_destroy(wl->cm_desc);
    }

    if (wl->fifo_manager) {
        wp_fifo_v1_destroy(wl->fifo);
        wp_fifo_manager_v1_destroy(wl->fifo_manager);
    }

    if (wl->commit_timing_manager) {
        wp_commit_timer_v1_destroy(wl->commit_timer);
        wp_commit_timing_manager_v1_destroy(wl->commit_timing_manager);
    }

    xdg_toplevel_destroy(wl->xdg_toplevel);
    xdg_surface_destroy(wl->xdg_surface);
    wl_surface_destroy(wl->surface);

    zwp_linux_dmabuf_v1_destroy(wl->dmabuf);

    wl_array_release(&wl->shm_formats);
    wl_shm_destroy(wl->shm);

    xdg_wm_base_destroy(wl->wm_base);

    if (wl->presentation)
        wp_presentation_destroy(wl->presentation);

    wl_compositor_destroy(wl->compositor);

    wl_keyboard_destroy(wl->keyboard);
    wl_seat_destroy(wl->seat);

    if (wl->color_manager)
        wp_color_manager_v1_destroy(wl->color_manager);

    struct wl_output_info *out, *out_tmp;
    wl_list_for_each_safe(out, out_tmp, &wl->outputs, node) {
        if (out->cm_output) {
            wp_image_description_v1_destroy(out->cm_desc);
            wp_color_management_output_v1_destroy(out->cm_output);
        }
        wl_output_release(out->output);
        free(out->make);
        free(out->model);
        free(out);
    }

    wl_display_flush(wl->display);
    wl_display_disconnect(wl->display);
}

static inline void
wl_info_dmabuf(const struct wl *wl)
{
    wl_log("dmabuf: main %s target, scanout %d, tranche count %d",
           wl->active.main_dev == wl->active.target_dev ? "==" : "!=", wl->active.scanout,
           wl->active.tranche_count);

    struct wl_array *dmabuf_iter;
    uint32_t idx = 0;
    wl_array_for_each(dmabuf_iter, &wl->active.formats) {
        const uint32_t fmt = *((uint64_t *)dmabuf_iter->data);
        uint32_t mod_count = dmabuf_iter->size / sizeof(uint64_t) - 1;
        wl_log("dmabuf format %d: '%.*s', modifier count %d", idx++, 4, (const char *)&fmt,
               mod_count);

        if (false) {
            const uint64_t *mod_iter;
            wl_array_for_each(mod_iter, dmabuf_iter) {
                if (mod_iter == dmabuf_iter->data)
                    continue;
                wl_log("  modifier 0x%" PRIx64, *mod_iter);
            }
        }
    }
}

static inline void
wl_info_shm(const struct wl *wl)
{
    const uint32_t *shm_iter;
    uint32_t idx = 0;
    wl_array_for_each(shm_iter, &wl->shm_formats) {
        uint32_t drm_format;
        switch (*shm_iter) {
        case WL_SHM_FORMAT_ARGB8888:
            drm_format = DRM_FORMAT_ARGB8888;
            break;
        case WL_SHM_FORMAT_XRGB8888:
            drm_format = DRM_FORMAT_XRGB8888;
            break;
        default:
            drm_format = *shm_iter;
            break;
        }
        wl_log("shm format %d: '%.*s'", idx++, 4, (const char *)&drm_format);
    }
}

static inline void
wl_info_outputs(const struct wl *wl)
{
    const struct wl_output_info *out;

    wl_list_for_each(out, &wl->outputs, node) {
        const char *primaries;
        switch (out->cm_primaries) {
        case WP_COLOR_MANAGER_V1_PRIMARIES_SRGB:
            primaries = "sRGB";
            break;
        case WP_COLOR_MANAGER_V1_PRIMARIES_BT2020:
            primaries = "BT.2020";
            break;
        case WP_COLOR_MANAGER_V1_PRIMARIES_DISPLAY_P3:
            primaries = "Display P3";
            break;
        default:
            primaries = "unknown";
            break;
        }

        const char *tf;
        switch (out->cm_tf) {
        case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22:
            tf = "Gamma 2.2";
            break;
        case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB:
            tf = "sRGB";
            break;
        case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ:
            tf = "ST2084 PQ";
            break;
        default:
            tf = "unknown";
            break;
        }

        wl_log("output %s %s: %dx%d @ %.2fHz (scale %dx), CMS %s (%s / %s)",
               out->make ? out->make : "unknown", out->model ? out->model : "unknown", out->width,
               out->height, out->refresh_rate / 1000.0, out->scale,
               out->cm_output ? "supported" : "unsupported", primaries, tf);
    }
}

static inline void
wl_info(const struct wl *wl)
{
    wl_info_outputs(wl);
    wl_info_shm(wl);
    wl_info_dmabuf(wl);
}

static inline void
wl_dispatch(const struct wl *wl)
{
    assert(wl->dispatch_ready);

    if (wl_display_dispatch(wl->display) < 0)
        wl_die("failed to dispatch display");
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

static inline bool
wl_query_shm_format_support(const struct wl *wl, uint32_t format, uint64_t modifier)
{
    if (modifier != DRM_FORMAT_MOD_LINEAR)
        return false;

    const uint32_t *iter;
    wl_array_for_each(iter, &wl->shm_formats) {
        if (*iter == format)
            return true;
    }

    return false;
}

static inline bool
wl_query_dmabuf_format_support(const struct wl *wl, uint32_t format, uint64_t modifier)
{
    const struct wl_array *iter;
    wl_array_for_each(iter, &wl->active.formats) {
        const uint64_t *iter2 = iter->data;
        if ((uint32_t)*iter2 == format) {
            wl_array_for_each(iter2, iter) {
                if (iter2 == iter->data)
                    continue;
                else if (*iter2 == modifier)
                    return true;
            }
            break;
        }
    }

    return false;
}

static inline struct wl_swapchain *
wl_create_swapchain(struct wl *wl,
                    uint32_t width,
                    uint32_t height,
                    uint32_t format,
                    uint64_t modifier,
                    uint32_t image_count)
{
    struct wl_swapchain *swapchain = calloc(1, sizeof(*swapchain));
    if (!swapchain)
        wl_die("failed to alloc swapchain");

    swapchain->images = calloc(image_count, sizeof(*swapchain->images));
    if (!swapchain->images)
        wl_die("failed to alloc swapchain images");

    swapchain->width = width;
    swapchain->height = height;
    swapchain->format = format;
    swapchain->modifier = modifier;
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
wl_add_swapchain_images_shm(struct wl *wl, struct wl_swapchain *swapchain)
{
    const uint32_t shm_format = wl_drm_format_to_shm_format(swapchain->format);
    if (!wl_query_shm_format_support(wl, shm_format, swapchain->modifier)) {
        wl_die("unsupported shm format '%.*s', modifier 0x%" PRIx64, 4,
               (const char *)&swapchain->format, swapchain->modifier);
    }

    const uint32_t img_cpp = u_drm_format_to_cpp(swapchain->format);
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

        img->buffer = wl_shm_pool_create_buffer(shm_pool, shm_offset, swapchain->width,
                                                swapchain->height, img_pitch, shm_format);
        wl_buffer_add_listener(img->buffer, &wl_buffer_listener, img);
        img->data = shm_ptr + shm_offset;
    }

    wl_shm_pool_destroy(shm_pool);
    close(shm_fd);

    swapchain->shm_size = shm_size;
}

static inline void
wl_add_swapchain_image_dmabuf(struct wl *wl,
                              struct wl_swapchain *swapchain,
                              struct wl_swapchain_image *img,
                              int fd,
                              const uint32_t *offsets,
                              const uint32_t *pitches,
                              uint32_t mem_plane_count)
{
    if (!wl_query_dmabuf_format_support(wl, swapchain->format, swapchain->modifier)) {
        wl_die("unsupported dmabuf format '%.*s', modifier 0x%" PRIx64, 4,
               (const char *)&swapchain->format, swapchain->modifier);
    }

    struct zwp_linux_buffer_params_v1 *params = zwp_linux_dmabuf_v1_create_params(wl->dmabuf);
    for (uint32_t i = 0; i < mem_plane_count; i++) {
        zwp_linux_buffer_params_v1_add(params, fd, i, offsets[i], pitches[i],
                                       swapchain->modifier >> 32, (uint32_t)swapchain->modifier);
    }
    img->buffer = zwp_linux_buffer_params_v1_create_immed(
        params, swapchain->width, swapchain->height, swapchain->format, 0);
    wl_buffer_add_listener(img->buffer, &wl_buffer_listener, img);
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
wl_surface_presentation_feedback(struct wl *wl)
{
    if (!wl->presentation)
        return;

    struct wp_presentation_feedback *feedback =
        wp_presentation_feedback(wl->presentation, wl->surface);

    wp_presentation_feedback_add_listener(feedback, &wp_presentation_feedback_listener, wl);
}

static inline void
wl_surface_fifo(struct wl *wl)
{
    if (!wl->fifo)
        return;

    /* add a dependency on the barrier bit */
    wp_fifo_v1_wait_barrier(wl->fifo);

    /* set the barrier bit which auto-clear on deadline latch */
    wp_fifo_v1_set_barrier(wl->fifo);
}

static inline void
wl_surface_commit_timing(struct wl *wl)
{
    if (!wl->commit_timer)
        return;

    wp_commit_timer_v1_set_timestamp(wl->commit_timer, 0, 0, 0);
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

    wl_surface_commit_timing(wl);
    wl_surface_fifo(wl);
    wl_surface_presentation_feedback(wl);

    /* Every surface commit creates a transaction from the pending state. The
     * transaction updates the active state only when its dependencies are
     * resolved. Explicit/implicit fencing, commit timing, fifo barrier wait,
     * etc. are all examples of dependencies.
     *
     * Transactions always update the active state in order. Deadline latch
     * uses the active state.
     */
    wl_surface_commit(wl->surface);
}

#endif /* WLUTIL_H */
