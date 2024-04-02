/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>
#include <stdbool.h>

#define PRINTFLIKE(f, a) __attribute__((format(printf, f, a)))
#define NORETURN __attribute__((noreturn))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

struct ff {
    AVFormatContext *input_ctx;
    const AVCodec *stream_codec;
    int stream_idx;
    AVStream *stream;

    AVBufferRef *hwdev_ctx;

    AVCodecContext *codec_ctx;

    AVPacket *packet;
    AVFrame *frame;
};

static inline void
ff_logv(const char *format, va_list ap)
{
    printf("FF: ");
    vprintf(format, ap);
    printf("\n");
}

static inline void NORETURN
ff_diev(const char *format, va_list ap)
{
    ff_logv(format, ap);
    abort();
}

static inline void PRINTFLIKE(1, 2) ff_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    ff_logv(format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(1, 2) NORETURN ff_die(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    ff_diev(format, ap);
    va_end(ap);
}

static inline void
ff_init_input(struct ff *ff, const char *filename)
{
    int ret;

    ret = avformat_open_input(&ff->input_ctx, filename, NULL, NULL);
    if (ret < 0)
        ff_die("failed to open %s: %d", filename, ret);

    ret = avformat_find_stream_info(ff->input_ctx, NULL);
    if (ret < 0)
        ff_die("failed to find stream info: %d", ret);

    ret = av_find_best_stream(ff->input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &ff->stream_codec, 0);
    if (ret < 0)
        ff_die("failed to find video stream: %d", ret);

    ff->stream_idx = ret;
    ff->stream = ff->input_ctx->streams[ff->stream_idx];

    ff_log("stream #%d, codec %s, size %dx%d", ff->stream_idx, ff->stream_codec->name,
           ff->stream->codecpar->width, ff->stream->codecpar->height);
}

static inline void
ff_init_hwdev(struct ff *ff, VADisplay dpy)
{
    const enum AVHWDeviceType hwdev_type = AV_HWDEVICE_TYPE_VAAPI;
    int ret;

    for (int i = 0;; i++) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(ff->stream_codec, i);
        if (!config)
            ff_die("failed to find hwdev type");
        else if (config->device_type != hwdev_type)
            continue;

        if (!(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX))
            ff_die("hwdev does not support hw_device_ctx");
        if (!(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX))
            ff_die("hwdev does not support hw_frames_ctx");

        break;
    }

    ff->hwdev_ctx = av_hwdevice_ctx_alloc(hwdev_type);
    if (!ff->hwdev_ctx)
        ff_die("failed to alloc hwdev context");

    AVHWDeviceContext *hwdev_ctx = (AVHWDeviceContext *)ff->hwdev_ctx->data;
    AVVAAPIDeviceContext *vadev_ctx = hwdev_ctx->hwctx;
    vadev_ctx->display = dpy;

    ret = av_hwdevice_ctx_init(ff->hwdev_ctx);
    if (ret < 0)
        ff_die("failed to init hwdev context");
}

static inline enum AVPixelFormat
ff_get_hwdev_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat pix_fmt = AV_PIX_FMT_VAAPI;

    while (*pix_fmts != AV_PIX_FMT_NONE) {
        if (*pix_fmts == pix_fmt)
            break;
        pix_fmts++;
    }
    if (*pix_fmts != pix_fmt)
        return AV_PIX_FMT_NONE;

    /* customization? */
#if 0
    struct ff *ff = ctx->opaque;
    int ret;

    ret = avcodec_get_hw_frames_parameters(ctx, ff->hwdev_ctx, pix_fmt, &ctx->hw_frames_ctx);
    if (ret < 0)
        ff_die("failed to alloc hwframe context");

    ret = av_hwframe_ctx_init(ctx->hw_frames_ctx);
    if (ret < 0)
        ff_die("failed to init hwframe context");
#endif

    return pix_fmt;
}

static inline void
ff_init_codec(struct ff *ff)
{
    int ret;

    ff->codec_ctx = avcodec_alloc_context3(ff->stream_codec);
    if (!ff->codec_ctx)
        ff_die("failed to alloc codec context");

    ret = avcodec_parameters_to_context(ff->codec_ctx, ff->stream->codecpar);
    if (ret < 0)
        ff_die("failed to init codec params: %d", ret);

    ff->codec_ctx->get_format = ff_get_hwdev_format;
    ff->codec_ctx->hw_device_ctx = av_buffer_ref(ff->hwdev_ctx);
    ff->codec_ctx->opaque = ff;

    ret = avcodec_open2(ff->codec_ctx, ff->stream_codec, NULL);
    if (ret < 0)
        ff_die("failed to open codec");
}

static inline void
ff_init(struct ff *ff, VADisplay dpy, const char *filename)
{
    ff_init_input(ff, filename);
    ff_init_hwdev(ff, dpy);
    ff_init_codec(ff);

    ff->packet = av_packet_alloc();
    if (!ff->packet)
        ff_die("failed to alloc packet");

    ff->frame = av_frame_alloc();
    if (!ff->frame)
        ff_die("failed to alloc frame");
}

static inline void
ff_cleanup(struct ff *ff)
{
    av_frame_free(&ff->frame);
    av_packet_free(&ff->packet);

    avcodec_free_context(&ff->codec_ctx);
    av_buffer_unref(&ff->hwdev_ctx);
    avformat_close_input(&ff->input_ctx);
}

static inline bool
ff_receive_frame(struct ff *ff)
{
    int ret = avcodec_receive_frame(ff->codec_ctx, ff->frame);
    if (ret >= 0) {
        return true;
    } else {
        if (ret != AVERROR_EOF && ret != AVERROR(EAGAIN))
            ff_die("failed to receive frame");
        return false;
    }
}

static inline bool
ff_decode_frame(struct ff *ff)
{
    if (ff_receive_frame(ff))
        return true;

    while (true) {
        int ret;

        ret = av_read_frame(ff->input_ctx, ff->packet);
        if (ret < 0) {
            /* flush */
            avcodec_send_packet(ff->codec_ctx, NULL);
            break;
        }

        if (ff->packet->stream_index != ff->stream_idx)
            continue;

        ret = avcodec_send_packet(ff->codec_ctx, ff->packet);
        if (ret < 0)
            ff_die("failed to send packet");

        av_packet_unref(ff->packet);

        if (ff_receive_frame(ff))
            return true;
    }

    return ff_receive_frame(ff);
}

static inline VASurfaceID
ff_get_frame_surface(struct ff *ff)
{
    return (uintptr_t)ff->frame->data[3];
}
