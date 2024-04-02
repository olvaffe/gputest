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

prog_glslang = find_program('glslangValidator')
prog_python = import('python').find_installation('python3')
prog_xxd = find_program('xxd')

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

warning_args = cc.get_supported_arguments([
  '-Wno-pedantic',
  '-Wno-missing-field-initializers',
  '-Wno-unused-parameter',
])

add_project_arguments(['-D_GNU_SOURCE', warning_args], language: ['c', 'cpp'])

dep_dl = cc.find_library('dl')
dep_m = cc.find_library('m', required: false)
dep_rt = cc.find_library('rt', required: false)

dep_drm = dependency('libdrm', required: false)
dep_ktx = dependency('Ktx', method: 'cmake', required: false)
dep_sdl2 = dependency('sdl2', required: false)
dep_wl_client = dependency('wayland-client', required: false)

dep_nativewindow = cc.find_library('nativewindow', required: false)
dep_gbm = dependency('gbm', required: false)

dep_libva = dependency('libva', required: false)
dep_libva_drm = dependency('libva-drm', required: false)

dep_libavcodec = dependency('libavcodec', required: false)
dep_libavformat = dependency('libavformat', required: false)
dep_libavutil = dependency('libavutil', required: false)

with_sk = false
skia_path = get_option('skia-path')
skia_path = fs.expanduser(skia_path)
if fs.is_absolute(skia_path) and fs.is_dir(skia_path)
  dep_libskia = cpp.find_library(
    'skia',
    dirs: [skia_path / 'out'],
    has_headers: ['include/core/SkGraphics.h'],
    header_include_directories: include_directories(skia_path),
  )

  dep_skia = declare_dependency(
    compile_args: ['-L' + skia_path / 'out', '-fno-rtti',
                   '-DSK_DEBUG', '-DSK_GANESH', '-DSK_GL', '-DSK_VULKAN'],
    dependencies: dep_libskia,
    include_directories: [skia_path],
  )

  with_sk = true
else
  message('skia-path ' + skia_path + ' is not an absolute directory')
endif

with_egl = false
if host_machine.system() == 'android'
  with_egl = dep_nativewindow.found()
else
  with_egl = dep_gbm.found()
endif

with_va = dep_libva.found() and dep_libva_drm.found()

with_vk = cc.has_header('vulkan/vulkan.h')

inc_include = include_directories('include')

subdir('util')

if with_egl
  subdir('egl')
endif

if with_sk
  subdir('sk')
endif

if with_va
  subdir('va')
endif

if with_vk
  subdir('vk')
endif
