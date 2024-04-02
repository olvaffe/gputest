# Copyright 2022 Google LLC
# SPDX-License-Identifier: MIT

project(
  'gputest',
  'c',
  version: '0.1',
  license: 'MIT',
  meson_version: '>=0.53',
  default_options: ['c_std=c11', 'warning_level=3'],
)

fs = import('fs')

prog_glslang = find_program('glslangValidator')
prog_python = import('python').find_installation('python3')
prog_xxd = find_program('xxd')

cc = meson.get_compiler('c')

warning_args = cc.get_supported_arguments([
  '-Wno-pedantic',
  '-Wno-missing-field-initializers',
  '-Wno-unused-parameter',
])

add_project_arguments(['-D_GNU_SOURCE', warning_args], language: 'c')

dep_dl = cc.find_library('dl')
dep_m = cc.find_library('m', required: false)
dep_rt = cc.find_library('rt', required: false)

dep_drm = dependency('libdrm', required: false)
dep_ktx = dependency('Ktx', method: 'cmake', required: false)
dep_sdl2 = dependency('sdl2', required: false)
dep_wl_client = dependency('wayland-client', required: false)

dep_nativewindow = cc.find_library('nativewindow', required: false)
dep_gbm = dependency('gbm', required: false)

with_egl = false
if host_machine.system() == 'android'
  with_egl = dep_nativewindow.found()
else
  with_egl = dep_gbm.found()
endif

inc_include = include_directories('include')

subdir('vk')
if with_egl
  subdir('egl')
endif
