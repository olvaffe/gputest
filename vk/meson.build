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
  'memory',
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

if idep_wlutil.found()
  tests += ['wl']
endif
if dep_sdl2.found()
  tests += ['sdl']
endif
if idep_drmutil.found()
  tests += ['drm']
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
  test_args = []
  if t == 'wl'
    test_deps += [idep_wlutil]
  elif t == 'sdl'
    test_deps += [dep_sdl2]
  elif t == 'drm'
    test_deps += [idep_drmutil]
  elif t == 'ktx'
    if dep_ktx.found()
      test_deps += [dep_ktx]
    else
      test_args += ['-DFAKEKTX']
    endif
  endif

  executable(
    t,
    sources: [t + '.c', test_incs],
    dependencies: test_deps,
    c_args: test_args,
  )
endforeach
