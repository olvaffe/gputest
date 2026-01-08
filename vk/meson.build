# Copyright 2022 Google LLC
# SPDX-License-Identifier: MIT

tests = [
  'bench_buffer',
  'bench_image',
  'buf_align',
  'cacheline',
  'clear',
  'clear_depth',
  'conv1d',
  'conv2d',
  'convlayer',
  'depth_resolve',
  'desc_buf',
  'display',
  'dma_heap',
  'dynamic_rendering',
  'dynamic_rendering_suspend_resume',
  'external',
  'formats',
  'gs',
  'ibo',
  'image',
  'info',
  'ktx',
  'loop',
  'mem',
  'mem_alloc',
  'msaa',
  'paced',
  'pipeline_stats',
  'push_const',
  'renderpass_ops',
  'sched',
  'separate_ds',
  'ssbo_max',
  'stencil',
  'storage_3d',
  'subgroup',
  'tess',
  'tex',
  'tex_depth',
  'tex_ubo',
  'timeline',
  'timestamp',
  'tri',
  'ubo',
  'xfer',
  'ycbcr',
]

if idep_androidutil.found()
  tests += ['android']
endif
if idep_drmutil.found()
  tests += ['residency']
  if idep_androidutil.found() or idep_gbmutil.found()
    tests += ['kms']
  endif
endif
if idep_spvutil.found()
  tests += ['compile']
endif
if idep_sdlutil.found()
  tests += ['sdl']
endif
if idep_wlutil.found()
  tests += ['wl']
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
        input: [file_hexdump, src],
        output: [dst],
        command: [prog_python, '@INPUT0@', 'binary', '@INPUT1@', '@OUTPUT@'],
      )
    endif
  endforeach

  test_deps = [idep_vkutil]
  test_args = []

  if t == 'android'
    test_deps += [idep_androidutil, dep_android]
  elif t == 'compile'
    test_deps += [idep_spvutil]
  elif t == 'kms'
    test_deps += [idep_drmutil]
    if idep_androidutil.found()
      test_deps += [idep_androidutil, dep_android]
    endif
    if idep_gbmutil.found()
      test_deps += [idep_gbmutil]
    endif
  elif t == 'ktx'
    if dep_ktx.found()
      test_deps += [dep_ktx]
    else
      test_args += ['-DFAKEKTX']
    endif
  elif t == 'residency'
    test_deps += [idep_drmutil]
  elif t == 'sdl'
    test_deps += [idep_sdlutil]
  elif t == 'wl'
    test_deps += [idep_wlutil]
  endif

  if t == 'android'
    shared_library(
      t,
      sources: [t + '.c', test_incs],
      dependencies: test_deps,
      c_args: test_args,
    )
  else
    executable(
      t,
      sources: [t + '.c', test_incs],
      dependencies: test_deps,
      c_args: test_args,
    )
  endif
endforeach
