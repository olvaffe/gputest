# Copyright 2022 Google LLC
# SPDX-License-Identifier: MIT

tests = [
  'buf_align',
  'cacheline',
  'clear',
  'clear_depth',
  'compute',
  'depth_resolve',
  'dynamic_rendering',
  'external',
  'formats',
  'gs',
  'image',
  'info',
  'ktx',
  'mem',
  'msaa',
  'pipeline_stats',
  'push_const',
  'renderpass_ops',
  'separate_ds',
  'stencil',
  'storage_3d',
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

if idep_spvutil.found()
  tests += ['compile']
endif
if dep_sdl2.found()
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
    test_deps += [dep_sdl2]
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
