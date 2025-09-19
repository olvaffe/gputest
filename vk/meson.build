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
  'msaa',
  'pipeline_stats',
  'push_const',
  'renderpass_ops',
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

  if t == 'compile'
    test_deps += [idep_spvutil]
  elif t == 'ktx'
    if dep_ktx.found()
      test_deps += [dep_ktx]
    else
      test_args += ['-DFAKEKTX']
    endif
  elif t == 'sdl'
    test_deps += [idep_sdlutil]
  elif t == 'wl'
    test_deps += [idep_wlutil]
  endif

  executable(
    t,
    sources: [t + '.c', test_incs],
    dependencies: test_deps,
    c_args: test_args,
  )
endforeach
