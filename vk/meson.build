# Copyright 2022 Google LLC
# SPDX-License-Identifier: MIT

wl_protocols_files = []
if dep_wl_client.found()
  protocols = [
    ['xdg-shell', 'stable/xdg-shell'],
    ['linux-dmabuf-unstable-v1', 'unstable/linux-dmabuf'],
  ]

  dep_wl_scanner = dependency('wayland-scanner')
  wl_scanner = find_program(dep_wl_scanner.get_variable(pkgconfig: 'wayland_scanner'))

  dep_wl_protocols = dependency('wayland-protocols')
  wl_protocols_dir = dep_wl_protocols.get_variable(pkgconfig: 'pkgdatadir')

  foreach proto : protocols
    xml_path = wl_protocols_dir / proto[1] / proto[0] + '.xml'

    wl_protocols_files += custom_target(proto[0] + ' client-header',
      command: [wl_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
      input: xml_path,
      output: proto[0] + '-protocol.h',
    )
    wl_protocols_files += custom_target(proto[0] + ' private-code',
      command: [wl_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
      input: xml_path,
      output: proto[0] + '-protocol.c',
    )
  endforeach
endif

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

if dep_wl_client.found()
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
    test_incs += wl_protocols_files
    test_deps += [dep_wl_client]
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
