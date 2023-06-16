# Copyright 2022 Google LLC
# SPDX-License-Identifier: MIT

project(
  'vktest',
  'c',
  version: '0.1',
  license: 'MIT',
  meson_version: '>=0.53',
  default_options: ['c_std=c11', 'warning_level=3'],
)

fs = import('fs')
prog_glslang = find_program('glslangValidator')
prog_xxd = find_program('xxd')

cc = meson.get_compiler('c')

warning_args = cc.get_supported_arguments([
  '-Wno-pedantic',
  '-Wno-missing-field-initializers',
  '-Wno-unused-parameter',
])

if not cc.has_header('vulkan/vulkan.h')
  error('failed to find vulkan/vulkan.h')
endif

dep_dl = cc.find_library('dl')
dep_m = cc.find_library('m', required: false)
dep_rt = cc.find_library('rt', required: false)
dep_sdl2 = dependency('sdl2', required: false)

add_project_arguments(['-D_GNU_SOURCE', warning_args], language: 'c')

idep_vkutil = declare_dependency(
  sources: ['vkutil.h'],
  dependencies: [dep_dl, dep_m, dep_rt],
  include_directories: ['include'],
)

tests = [
  'buf_align',
  'cacheline',
  'clear',
  'clear_depth',
  'compute',
  'formats',
  'gs',
  'info',
  'msaa',
  'push_const',
  'renderpass_ops',
  'tess',
  'tex',
  'tex_depth',
  'tex_ubo',
  'timestamp',
  'tri',
  'ubo',
  'xfer',
  'ycbcr',
]

if dep_sdl2.found()
  tests += ['sdl']
endif

foreach t : tests
  test_incs = []

  foreach suffix : ['vert', 'tesc', 'tese', 'geom', 'frag', 'comp']
    src = t + '.' + suffix
    dst = t + '_test.' + suffix + '.inc'
    if fs.exists(src)
      test_incs += custom_target(
        dst,
        input: [src],
        output: [dst],
        command: [prog_glslang, '--quiet', '--target-env', 'vulkan1.1', '-x',
                  '-o', '@OUTPUT@', '@INPUT@']
      )
    endif
  endforeach

  foreach suffix : ['ppm']
    src = t + '.' + suffix
    dst = t + '_test.' + suffix + '.inc'
    if fs.exists(src)
      test_incs += custom_target(
        dst,
        input: [src],
        output: [dst],
        command: [prog_xxd, '-i', '-n', t + '_test_ppm', '@INPUT@'],
        capture: true,
      )
    endif
  endforeach

  test_deps = [idep_vkutil]
  if t == 'sdl'
    test_deps += [dep_sdl2]
  endif

  executable(
    t,
    sources: [t + '.c', test_incs],
    dependencies: test_deps,
  )
endforeach
