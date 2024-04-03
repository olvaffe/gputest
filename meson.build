# Copyright 2022 Google LLC
# SPDX-License-Identifier: MIT

project(
  'gputest',
  ['c', 'cpp'],
  version: '0.1',
  license: 'MIT',
  meson_version: '>=1.1.0',
  default_options: ['c_std=c11', 'warning_level=3'],
)

fs = import('fs')

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

warning_args = cc.get_supported_arguments([
  '-Wno-pedantic',
  '-Wno-missing-field-initializers',
  '-Wno-unused-parameter',
])
add_project_arguments(['-D_GNU_SOURCE', warning_args], language: ['c', 'cpp'])

prog_glslang = find_program('glslangValidator')
prog_python = import('python').find_installation('python3')
prog_xxd = find_program('xxd')

dep_not_found = dependency('', required : false)

dep_dl = cc.find_library('dl')
dep_m = cc.find_library('m', required: false)
dep_rt = cc.find_library('rt', required: false)

# for drmutil and vautil
dep_drm = dependency('libdrm', required: false)

# for eglutil and gbmutil
dep_gbm = dependency('gbm', required: false)
dep_nativewindow = cc.find_library('nativewindow', required: false)

# for ffutil
dep_libavcodec = dependency('libavcodec', required: false)
dep_libavformat = dependency('libavformat', required: false)
dep_libavutil = dependency('libavutil', required: false)

# for skutil
dep_skia = dep_not_found
dep_egl = dep_not_found
dep_glesv2 = dep_not_found
skia_path = get_option('skia-path')
skia_path = fs.expanduser(skia_path)
if fs.is_absolute(skia_path) and fs.is_dir(skia_path)
  dep_skia = cpp.find_library(
    'skia',
    dirs: [skia_path / 'out'],
    has_headers: ['include/core/SkGraphics.h'],
    header_include_directories: include_directories(skia_path),
  )

  dep_egl = dependency('egl')
  dep_glesv2 = dependency('glesv2')
endif

# for vautil
dep_libva = dependency('libva', required: false)
dep_libva_drm = dependency('libva-drm', required: false)

# for wlutil
dep_wl_client = dependency('wayland-client', required: false)
dep_wl_protocols = dependency('wayland-protocols', required: false)
dep_wl_scanner = dependency('wayland-scanner', required: false)

dep_sdl2 = dependency('sdl2', required: false)
dep_ktx = dependency('Ktx', method: 'cmake', required: false)

inc_include = include_directories('include')

subdir('util')

if idep_eglutil.found()
  subdir('egl')
endif

if idep_skutil.found()
  subdir('sk')
endif

if idep_vautil.found()
  subdir('va')
endif

if idep_vkutil.found()
  subdir('vk')
endif

summary(util_summary, section: 'Enabled utils')
