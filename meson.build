# Copyright 2022 Google LLC
# SPDX-License-Identifier: MIT

project(
  'gputest',
  ['c', 'cpp'],
  version: '0.1',
  license: 'MIT',
  meson_version: '>=1.1.0',
  default_options: ['c_std=c11', 'cpp_std=c++17', 'warning_level=3'],
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

dep_not_found = dependency('', required : false)

dep_dl = cc.find_library('dl')
dep_m = cc.find_library('m', required: false)
dep_rt = cc.find_library('rt', required: false)

# for androidutil
dep_nativewindow = cc.find_library(
  'nativewindow',
  has_headers: ['android/hardware_buffer.h'],
  required: false
)

# for drmutil and vautil
dep_drm = dependency('libdrm', required: false)

# for ffutil
dep_libavcodec = dependency('libavcodec', required: false)
dep_libavformat = dependency('libavformat', required: false)
dep_libavutil = dependency('libavutil', required: false)

# for gbmutil
dep_gbm = dependency('gbm', required: false)

# for sdlutil
dep_sdl2 = dependency('sdl2', required: false)

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

# for spvutil
dep_spirv_tools = dependency('SPIRV-Tools', required: false)

dep_glslang = cpp.find_library(
    'glslang',
    has_headers: [
      'glslang/Include/glslang_c_interface.h',
      'glslang/Public/resource_limits_c.h',
    ],
    required: false,
)

dep_clspv = dep_not_found
clspv_path = get_option('clspv-path')
clspv_path = fs.expanduser(clspv_path)
if fs.is_absolute(clspv_path) and fs.is_dir(clspv_path)
  dep_clspv = cpp.find_library(
    'clspv_core',
    dirs: [clspv_path / 'out' / 'lib'],
    has_headers: ['clspv/Compiler.h'],
    header_include_directories: include_directories(clspv_path / 'include'),
  )
endif

dep_llvm = dep_not_found
dep_clang = dep_not_found
dep_llvm_spirv = dependency('LLVMSPIRVLib', required: false)
if dep_llvm_spirv.found()
  ver_arr = dep_llvm_spirv.version().split('.')
  ver_maj = ver_arr[0].to_int()
  ver_min = ver_arr[1].to_int()
  version = [
    '>= @0@.@1@'.format(ver_maj, ver_min),
    '< @0@.@1@'.format(ver_maj, ver_min + 1)
  ]

  dep_llvm = dependency('LLVM', version: version, method: 'cmake')
  dep_clang = dependency('Clang', version: version, modules: ['clang-cpp'], method: 'cmake')
endif

dep_spirv_reflect = dep_not_found
spirv_reflect_path = get_option('spirv-reflect-path')
spirv_reflect_path = fs.expanduser(spirv_reflect_path)
if fs.is_absolute(spirv_reflect_path) and fs.is_dir(spirv_reflect_path)
  dep_spirv_reflect = cpp.find_library(
    'spirv-reflect-static',
    dirs: [spirv_reflect_path / 'out'],
    has_headers: ['spirv_reflect.h'],
    header_include_directories: include_directories(spirv_reflect_path),
  )
endif

# for vautil
dep_libva = dependency('libva', required: false)
dep_libva_drm = dependency('libva-drm', required: false)

# for wlutil
dep_wl_client = dependency('wayland-client', required: false)
dep_wl_protocols = dependency('wayland-protocols', required: false)
dep_wl_scanner = dependency('wayland-scanner', required: false)

dep_ktx = dependency('Ktx', method: 'cmake', required: false)

inc_include = include_directories('include')

subdir('util')
subdir('misc')

if idep_clutil.found()
  subdir('cl')
endif

if idep_eglutil.found()
  subdir('egl')
endif

if idep_skutil.found()
  subdir('sk')
endif

if idep_drmutil.found() and idep_vautil.found()
  subdir('va')
endif

if idep_vkutil.found()
  subdir('vk')
endif

summary(util_summary, section: 'Enabled utils')
