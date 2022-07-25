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

add_project_arguments(['-D_GNU_SOURCE', warning_args], language: 'c')

idep_vkutil = declare_dependency(
  sources: ['vkutil.h'],
  dependencies: [dep_dl, dep_m],
)

executable(
  'clear',
  sources: ['clear.c'],
  dependencies: [idep_vkutil],
)

tests = [
  'tri',
  'tex',
  'tex_depth',
  'ubo',
  'msaa',
  'gs',
  'tess',
]

foreach t : tests
  spirv_incs = []
  foreach stage : ['vert', 'tesc', 'tese', 'geom', 'frag']
    glsl = t + '.' + stage
    if fs.exists(glsl)
      spirv_incs += custom_target(
        t + '.' + stage + '.inc',
        input: [t + '.' + stage],
        output: [t + '.' + stage + '.inc'],
        command: [prog_glslang, '--quiet', '--target-env', 'vulkan1.1', '-x',
                  '-o', '@OUTPUT@', '@INPUT@']
      )
    endif
  endforeach

  executable(
    t,
    sources: [t + '.c', spirv_incs],
    dependencies: [idep_vkutil],
  )
endforeach
