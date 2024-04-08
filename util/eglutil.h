/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef EGLUTIL_H
#define EGLUTIL_H

#define EGL_EGL_PROTOTYPES 0
#define GL_GLES_PROTOTYPES 0

#include "util.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
/* clang-format off */
#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>
/* clang-format on */
#include <dlfcn.h>
#include <fcntl.h>
#include <strings.h>
#include <unistd.h>

#ifdef __ANDROID__
#define LIBEGL_NAME "libEGL.so"
#else
#define LIBEGL_NAME "libEGL.so.1"
#endif

struct egl_gl {
#define PFN_GL(proc, name) PFNGL##proc##PROC name;
#include "eglutil_entrypoints.inc"
};

struct egl_drm_format {
    int drm_format;
    int drm_modifier_count;
    const EGLuint64KHR *drm_modifiers;
    const EGLBoolean *external_only;
};

struct egl_init_params {
    EGLint pbuffer_width;
    EGLint pbuffer_height;
};

struct egl {
    struct egl_init_params params;

    struct {
        void *handle;

#define PFN_EGL(proc, name) PFNEGL##proc##PROC name;
#include "eglutil_entrypoints.inc"
        struct egl_gl gl;

        const char *client_exts;
    };

    EGLDeviceEXT dev;
    EGLDisplay dpy;
    EGLint major;
    EGLint minor;

    const char *dpy_exts;
    bool KHR_no_config_context;
    bool EXT_image_dma_buf_import;
    bool EXT_image_dma_buf_import_modifiers;
    bool ANDROID_get_native_client_buffer;
    bool ANDROID_image_native_buffer;

    EGLConfig config;
    EGLSurface surf;

    EGLContext ctx;

    int drm_format_count;
    struct egl_drm_format **drm_formats;

    const char *gl_exts;
};

struct egl_framebuffer {
    GLuint fbo;
    GLuint tex;
};

struct egl_program {
    GLuint vs;
    GLuint fs;
    GLuint prog;
};

struct egl_image_info {
    EGLContext ctx;
    EGLenum target;
    EGLClientBuffer buf;

    int dma_buf_fd;
    int width;
    int height;
    int drm_format;
    uint64_t drm_modifier;
    int mem_plane_count;
    int offsets[4];
    int pitches[4];
};

struct egl_image {
    EGLImage img;
};

static inline void PRINTFLIKE(1, 2) egl_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_logv("EGL", format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(1, 2) NORETURN egl_die(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_diev("EGL", format, ap);
    va_end(ap);
}

static inline void
egl_check(struct egl *egl, const char *where)
{
    const EGLint egl_err = egl->GetError();
    if (egl_err != EGL_SUCCESS)
        egl_die("%s: egl has error 0x%04x", where, egl_err);

    if (egl->ctx) {
        const GLenum gl_err = egl->gl.GetError();
        if (gl_err != GL_NO_ERROR)
            egl_die("%s: gl has error 0x%04x", where, gl_err);
    }
}

static inline void
egl_init_library_dispatch(struct egl *egl)
{
    /* we assume EGL 1.5, which includes EGL_EXT_client_extensions and
     * EGL_KHR_client_get_all_proc_addresses
     */
#define PFN_EGL_EXT(proc, name) egl->name = (PFNEGL##proc##PROC)egl->GetProcAddress("egl" #name);
#define PFN_GL_EXT(proc, name) egl->gl.name = (PFNGL##proc##PROC)egl->GetProcAddress("gl" #name);
#define PFN_EGL(proc, name)                                                                      \
    PFN_EGL_EXT(proc, name)                                                                      \
    if (!egl->name)                                                                              \
        egl_die("no egl" #name);
#define PFN_GL(proc, name)                                                                       \
    PFN_GL_EXT(proc, name)                                                                       \
    if (!egl->gl.name)                                                                           \
        egl_die("no gl" #name);
#include "eglutil_entrypoints.inc"
}

static inline void
egl_init_library(struct egl *egl)
{
    egl->handle = dlopen(LIBEGL_NAME, RTLD_LOCAL | RTLD_LAZY);
    if (!egl->handle)
        egl_die("failed to load %s: %s", LIBEGL_NAME, dlerror());

    const char gipa_name[] = "eglGetProcAddress";
    egl->GetProcAddress = dlsym(egl->handle, gipa_name);
    if (!egl->GetProcAddress)
        egl_die("failed to find %s: %s", gipa_name, dlerror());

    egl_init_library_dispatch(egl);

    egl->client_exts = egl->QueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (!egl->client_exts) {
#ifdef __ANDROID__
        egl_log("no client extension");
        egl->client_exts = "";
        egl->GetError();
#else
        egl_die("no client extension");
#endif
    }
}

static inline void
egl_init_display_extensions(struct egl *egl)
{
    egl->dpy_exts = egl->QueryString(egl->dpy, EGL_EXTENSIONS);

    egl->KHR_no_config_context = strstr(egl->dpy_exts, "EGL_KHR_no_config_context");
    egl->EXT_image_dma_buf_import = strstr(egl->dpy_exts, "EGL_EXT_image_dma_buf_import");
    egl->EXT_image_dma_buf_import_modifiers =
        strstr(egl->dpy_exts, "EGL_EXT_image_dma_buf_import_modifiers");
    egl->ANDROID_get_native_client_buffer =
        strstr(egl->dpy_exts, "EGL_ANDROID_get_native_client_buffer");
    egl->ANDROID_image_native_buffer = strstr(egl->dpy_exts, "EGL_ANDROID_image_native_buffer");
}

static inline void
egl_init_display(struct egl *egl)
{
    const bool EXT_device_enumeration = strstr(egl->client_exts, "EGL_EXT_device_enumeration");
    const bool EXT_device_query = strstr(egl->client_exts, "EGL_EXT_device_query");
    const bool EXT_platform_device = strstr(egl->client_exts, "EGL_EXT_platform_device");
    const bool KHR_platform_android = strstr(egl->client_exts, "EGL_KHR_platform_android");

    if (EXT_device_enumeration && EXT_device_query && EXT_platform_device) {
        egl_log("using platform device");

        EGLDeviceEXT devs[16];
        EGLint count;
        if (!egl->QueryDevicesEXT(ARRAY_SIZE(devs), devs, &count))
            egl_die("failed to query devices");

        egl->dev = EGL_NO_DEVICE_EXT;
        for (int i = 0; i < count; i++) {
            const char *exts = egl->QueryDeviceStringEXT(devs[i], EGL_EXTENSIONS);

            /* EGL_EXT_device_drm_render_node */
            if (!strstr(exts, "EGL_EXT_device_drm_render_node"))
                continue;

            const bool swrast = strstr(exts, "software");
            if (!swrast) {
                egl->dev = devs[i];
                break;
            }
        }
        if (egl->dev == EGL_NO_DEVICE_EXT)
            egl_die("failed to find a hw rendernode device");

        egl->dpy = egl->GetPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, egl->dev, NULL);
    } else if (KHR_platform_android) {
        egl_log("using platform android");

        egl->dev = EGL_NO_DEVICE_EXT;
        egl->dpy = egl->GetPlatformDisplay(EGL_PLATFORM_ANDROID_KHR, EGL_DEFAULT_DISPLAY, NULL);
    } else {
        egl_log("using EGL_DEFAULT_DISPLAY");
        egl->dev = EGL_NO_DEVICE_EXT;
        egl->dpy = egl->GetDisplay(EGL_DEFAULT_DISPLAY);
    }

    if (egl->dpy == EGL_NO_DISPLAY)
        egl_die("failed to get platform display");

    if (!egl->Initialize(egl->dpy, &egl->major, &egl->minor))
        egl_die("failed to initialize display");

    egl_init_display_extensions(egl);

    if (egl->major != 1 || egl->minor < 5) {
#ifdef __ANDROID__
        egl_log("fixing up for EGL %d.%d", egl->major, egl->minor);
        if (!strstr(egl->dpy_exts, "EGL_KHR_image_base"))
            egl_die("no EGL_KHR_image_base");
        egl->CreateImage = (PFNEGLCREATEIMAGEPROC)egl->GetProcAddress("eglCreateImageKHR");
        egl->DestroyImage = (PFNEGLDESTROYIMAGEPROC)egl->GetProcAddress("eglDestroyImageKHR");
#else
        egl_die("EGL 1.5 is required");
#endif
    }
}

static inline void
egl_init_config_and_surface(struct egl *egl)
{
    const bool with_pbuffer = egl->params.pbuffer_width && egl->params.pbuffer_height;
    if (egl->KHR_no_config_context && !with_pbuffer) {
        egl_log("using EGL_NO_CONFIG_KHR");
        egl->config = EGL_NO_CONFIG_KHR;
        return;
    }

    const EGLint config_attrs[] = {
        EGL_RED_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_BLUE_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8,
        EGL_RENDERABLE_TYPE,
        EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,
        with_pbuffer ? EGL_PBUFFER_BIT : 0,
        EGL_NONE,
    };

    EGLint count;
    if (!egl->ChooseConfig(egl->dpy, config_attrs, &egl->config, 1, &count) || !count)
        egl_die("failed to choose a config");

    if (!with_pbuffer) {
        egl_log("using EGL_NO_SURFACE");
        egl->surf = EGL_NO_SURFACE;
        return;
    }

    const EGLint surf_attrs[] = {
        EGL_WIDTH, egl->params.pbuffer_width, EGL_HEIGHT, egl->params.pbuffer_height, EGL_NONE,
    };

    egl->surf = egl->CreatePbufferSurface(egl->dpy, egl->config, surf_attrs);
    if (egl->surf == EGL_NO_SURFACE)
        egl_die("failed to create pbuffer surface");
}

static inline void
egl_init_context(struct egl *egl)
{
    if (egl->QueryAPI() != EGL_OPENGL_ES_API)
        egl_die("current api is not GLES");

    const EGLint ctx_attrs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE,
    };

    EGLContext ctx = egl->CreateContext(egl->dpy, egl->config, EGL_NO_CONTEXT, ctx_attrs);
    if (ctx == EGL_NO_CONTEXT)
        egl_die("failed to create a context");

    if (!egl->MakeCurrent(egl->dpy, egl->surf, egl->surf, ctx))
        egl_die("failed to make context current");

    egl->ctx = ctx;
}

static inline void
egl_init_drm_formats(struct egl *egl)
{
    if (!egl->EXT_image_dma_buf_import_modifiers)
        return;

    EGLint fmt_count;
    if (!egl->QueryDmaBufFormatsEXT(egl->dpy, 0, NULL, &fmt_count))
        egl_die("failed to get dma-buf format count");

    struct egl_drm_format **fmts = malloc(sizeof(*fmts) * fmt_count);
    EGLint *drm_fmts = malloc(sizeof(*drm_fmts) * fmt_count);
    if (!fmts || !drm_fmts)
        egl_die("failed to alloc fmts");

    if (!egl->QueryDmaBufFormatsEXT(egl->dpy, fmt_count, drm_fmts, &fmt_count))
        egl_die("failed to get dma-buf formats");

    for (int i = 0; i < fmt_count; i++) {
        const EGLint drm_fmt = drm_fmts[i];

        EGLint mod_count;
        if (!egl->QueryDmaBufModifiersEXT(egl->dpy, drm_fmt, 0, NULL, NULL, &mod_count))
            egl_die("failed to get dma-buf modifier count");

        struct egl_drm_format *fmt =
            malloc(sizeof(*fmt) + sizeof(fmt->drm_modifiers) * mod_count +
                   sizeof(fmt->external_only) * mod_count);
        if (!fmt)
            egl_die("failed to alloc fmt");
        EGLuint64KHR *drm_modifiers = (EGLuint64KHR *)(fmt + 1);
        EGLBoolean *external_only = (EGLBoolean *)(drm_modifiers + mod_count);

        if (!egl->QueryDmaBufModifiersEXT(egl->dpy, drm_fmt, mod_count, drm_modifiers,
                                          external_only, &mod_count))
            egl_die("failed to get dma-buf modifiers");

        fmt->drm_format = drm_fmt;
        fmt->drm_modifier_count = mod_count;
        fmt->drm_modifiers = drm_modifiers;
        fmt->external_only = external_only;
        fmts[i] = fmt;
    }

    free(drm_fmts);

    egl->drm_format_count = fmt_count;
    egl->drm_formats = fmts;
}

static inline void
egl_init_gl(struct egl *egl)
{
    egl->gl_exts = (const char *)egl->gl.GetString(GL_EXTENSIONS);
    if (!egl->gl_exts)
        egl_die("no GLES extensions");
}

static inline void
egl_init(struct egl *egl, const struct egl_init_params *params)
{
    memset(egl, 0, sizeof(*egl));

    if (params)
        egl->params = *params;

    egl_init_library(egl);
    egl_check(egl, "init library");

    egl_init_display(egl);
    egl_check(egl, "init display");

    egl_init_config_and_surface(egl);
    egl_check(egl, "init config and surface");

    egl_init_context(egl);
    egl_check(egl, "init context");

    egl_init_drm_formats(egl);
    egl_check(egl, "init formats");

    egl_init_gl(egl);
    egl_check(egl, "init gl");
}

static inline void
egl_cleanup(struct egl *egl)
{
    egl_check(egl, "cleanup");

    if (egl->drm_format_count) {
        for (int i = 0; i < egl->drm_format_count; i++)
            free(egl->drm_formats[i]);
        free(egl->drm_formats);
    }

    egl->MakeCurrent(egl->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    egl->DestroyContext(egl->dpy, egl->ctx);
    if (egl->surf != EGL_NO_SURFACE)
        egl->DestroySurface(egl->dpy, egl->surf);

    egl->Terminate(egl->dpy);
    egl->ReleaseThread();

    dlclose(egl->handle);
}

static inline const char *
egl_get_drm_render_node(struct egl *egl)
{
    if (egl->dev == EGL_NO_DEVICE_EXT)
        egl_die("no EGLDeviceEXT");

    const char *path = egl->QueryDeviceStringEXT(egl->dev, EGL_DRM_RENDER_NODE_FILE_EXT);
    if (!path)
        egl_die("failed to query drm render node");

    return path;
}

static inline void
egl_dump_drm_formats(struct egl *egl)
{
    for (int i = 0; i < egl->drm_format_count; i++) {
        const struct egl_drm_format *fmt = egl->drm_formats[i];

        egl_log("format %d: %c%c%c%c (0x%08x)", i, (fmt->drm_format >> 0) & 0xff,
                (fmt->drm_format >> 8) & 0xff, (fmt->drm_format >> 16) & 0xff,
                (fmt->drm_format >> 24) & 0xff, fmt->drm_format);
        for (int j = 0; j < fmt->drm_modifier_count; j++) {
            egl_log("  modifier 0x%016" PRIx64 " external only %d", fmt->drm_modifiers[j],
                    fmt->external_only[j]);
        }
    }
}

static inline void
egl_dump_image(struct egl *egl, int width, int height, const char *filename)
{
    const GLenum format = GL_RGBA;
    const GLenum type = GL_UNSIGNED_BYTE;
    const GLsizei size = width * height * 4;

    char *data = malloc(size);
    if (!data)
        egl_die("failed to alloc readback buf");

    egl->gl.ReadnPixels(0, 0, width, height, format, type, size, data);
    egl_check(egl, "dump");

    u_write_ppm(filename, data, width, height);

    free(data);
}

static inline void
egl_teximage_2d_from_ppm(struct egl *egl, GLenum target, const void *ppm_data, size_t ppm_size)
{
    struct egl_gl *gl = &egl->gl;

    int width;
    int height;
    ppm_data = u_parse_ppm(ppm_data, ppm_size, &width, &height);

    void *texels = malloc(width * height * 4);
    if (!texels)
        egl_die("failed to alloc texels");

    const uint8_t *rgb = ppm_data;
    uint8_t *rgba = texels;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            rgba[0] = rgb[0];
            rgba[1] = rgb[1];
            rgba[2] = rgb[2];
            rgba[3] = 0xff;
            rgba += 4;
            rgb += 3;
        }
    }

    gl->TexImage2D(target, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);

    free(texels);
}

static inline struct egl_framebuffer *
egl_create_framebuffer(struct egl *egl, int width, int height)
{
    struct egl_gl *gl = &egl->gl;

    struct egl_framebuffer *fb = calloc(1, sizeof(*fb));
    if (!fb)
        egl_die("failed to alloc fb");

    const GLenum target = GL_FRAMEBUFFER;
    const GLenum textarget = GL_TEXTURE_2D;
    const GLenum att = GL_COLOR_ATTACHMENT0;

    gl->GenTextures(1, &fb->tex);
    gl->BindTexture(textarget, fb->tex);
    gl->TexStorage2D(textarget, 1, GL_RGBA8, width, height);
    gl->BindTexture(textarget, 0);

    gl->GenFramebuffers(1, &fb->fbo);
    gl->BindFramebuffer(target, fb->fbo);
    gl->FramebufferTexture(target, att, fb->tex, 0);

    if (gl->CheckFramebufferStatus(target) != GL_FRAMEBUFFER_COMPLETE)
        egl_die("incomplete fbo");

    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);

    return fb;
}

static inline void
egl_destroy_framebuffer(struct egl *egl, struct egl_framebuffer *fb)
{
    struct egl_gl *gl = &egl->gl;

    gl->DeleteTextures(1, &fb->tex);
    gl->DeleteFramebuffers(1, &fb->fbo);

    free(fb);
}

static inline GLuint
egl_compile_shader(struct egl *egl, GLenum type, const char *glsl)
{
    struct egl_gl *gl = &egl->gl;

    GLuint sh = gl->CreateShader(type);
    gl->ShaderSource(sh, 1, &glsl, NULL);
    gl->CompileShader(sh);

    GLint val;
    gl->GetShaderiv(sh, GL_COMPILE_STATUS, &val);
    if (val != GL_TRUE) {
        char info_log[1024];
        gl->GetShaderInfoLog(sh, sizeof(info_log), NULL, info_log);
        egl_die("failed to compile shader: %s", info_log);
    }

    return sh;
}

static inline GLuint
egl_link_program(struct egl *egl, const GLuint *shaders, int count)
{
    struct egl_gl *gl = &egl->gl;

    GLuint prog = gl->CreateProgram();
    for (int i = 0; i < count; i++)
        gl->AttachShader(prog, shaders[i]);
    gl->LinkProgram(prog);

    GLint val;
    gl->GetProgramiv(prog, GL_LINK_STATUS, &val);
    if (val != GL_TRUE) {
        char info_log[1024];
        gl->GetProgramInfoLog(prog, sizeof(info_log), NULL, info_log);
        egl_die("failed to link program: %s", info_log);
    }

    return prog;
}

static inline struct egl_program *
egl_create_program(struct egl *egl, const char *vs_glsl, const char *fs_glsl)
{
    struct egl_program *prog = calloc(1, sizeof(*prog));
    if (!prog)
        egl_die("failed to alloc prog");

    prog->vs = egl_compile_shader(egl, GL_VERTEX_SHADER, vs_glsl);
    prog->fs = egl_compile_shader(egl, GL_FRAGMENT_SHADER, fs_glsl);

    const GLuint shaders[] = { prog->vs, prog->fs };
    prog->prog = egl_link_program(egl, shaders, ARRAY_SIZE(shaders));

    return prog;
}

static inline void
egl_destroy_program(struct egl *egl, struct egl_program *prog)
{
    struct egl_gl *gl = &egl->gl;

    gl->DeleteProgram(prog->prog);
    gl->DeleteShader(prog->vs);
    gl->DeleteShader(prog->fs);

    free(prog);
}

static inline struct egl_image *
egl_create_image(struct egl *egl, const struct egl_image_info *info)
{
    EGLAttrib attrs[64];
    uint32_t attr_count = 0;

    attrs[attr_count++] = EGL_IMAGE_PRESERVED;
    attrs[attr_count++] = EGL_TRUE;

    switch (info->target) {
    case EGL_NATIVE_BUFFER_ANDROID:
        if (!egl->ANDROID_image_native_buffer)
            egl_die("no native buffer import support");
        break;
    case EGL_LINUX_DMA_BUF_EXT:
        if (!egl->EXT_image_dma_buf_import || !egl->EXT_image_dma_buf_import_modifiers)
            egl_die("no dma-buf import support");

        attrs[attr_count++] = EGL_WIDTH;
        attrs[attr_count++] = info->width;
        attrs[attr_count++] = EGL_HEIGHT;
        attrs[attr_count++] = info->height;
        attrs[attr_count++] = EGL_LINUX_DRM_FOURCC_EXT;
        attrs[attr_count++] = info->drm_format;

        if (info->mem_plane_count > 4)
            egl_die("unexpected plane count");
        for (int i = 0; i < info->mem_plane_count; i++) {
            if (i < 3) {
                attrs[attr_count++] = EGL_DMA_BUF_PLANE0_FD_EXT + 3 * i;
                attrs[attr_count++] = info->dma_buf_fd;
                attrs[attr_count++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT + 3 * i;
                attrs[attr_count++] = info->offsets[i];
                attrs[attr_count++] = EGL_DMA_BUF_PLANE0_PITCH_EXT + 3 * i;
                attrs[attr_count++] = info->pitches[i];
            } else {
                attrs[attr_count++] = EGL_DMA_BUF_PLANE3_FD_EXT;
                attrs[attr_count++] = info->dma_buf_fd;
                attrs[attr_count++] = EGL_DMA_BUF_PLANE3_OFFSET_EXT;
                attrs[attr_count++] = info->offsets[i];
                attrs[attr_count++] = EGL_DMA_BUF_PLANE3_PITCH_EXT;
                attrs[attr_count++] = info->pitches[i];
            }
            attrs[attr_count++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT + 2 * i;
            attrs[attr_count++] = (EGLAttrib)info->drm_modifier;
            attrs[attr_count++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT + 2 * i;
            attrs[attr_count++] = (EGLAttrib)(info->drm_modifier >> 32);
        }
        break;
    default:
        egl_die("bad image target");
        break;
    }

    attrs[attr_count++] = EGL_NONE;
    assert(attr_count <= ARRAY_SIZE(attrs));

    struct egl_image *img = calloc(1, sizeof(*img));
    if (!img)
        egl_die("failed to alloc img");

    img->img = egl->CreateImage(egl->dpy, info->ctx, info->target, info->buf, attrs);
    if (img->img == EGL_NO_IMAGE)
        egl_die("failed to create img");

    return img;
}

static inline void
egl_destroy_image(struct egl *egl, struct egl_image *img)
{
    egl->DestroyImage(egl->dpy, img->img);
    free(img);
}

#endif /* EGLUTIL_H */
