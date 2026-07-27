# Copyright 2026 Google LLC
# SPDX-License-Identifier: MIT

d3d11_tests = [
  'info',
  'tex',
  'tri',
]

foreach t : d3d11_tests
  test_src = t + '.c'
  test_incs = []

  hlsl_src = t + '.hlsl'
  if fs.exists(hlsl_src)
    foreach stage : ['vs', 'ps']
      hlsl_bc = custom_target(
        t + '_test.' + stage + '.dxbc',
        input: [hlsl_src],
        output: [t + '_test.' + stage + '.dxbc'],
        command: [
          prog_vkd3d_compiler,
          '-x', 'hlsl',
          '-b', 'dxbc-tpf',
          '-p', stage + '_5_0',
          '-e', stage + '_main',
          '-o', '@OUTPUT@',
          '@INPUT@',
        ],
      )
      test_incs += custom_target(
        t + '_test.' + stage + '.h',
        input: [file_hexdump, hlsl_bc],
        output: [t + '_test.' + stage + '.h'],
        command: [prog_python, '@INPUT0@', 'binary', '@INPUT1@', '@OUTPUT@'],
      )
    endforeach
  endif

  ppm_src = t + '.ppm'
  if fs.exists(ppm_src)
    dst = t + '_test.ppm.inc'
    test_incs += custom_target(
      dst,
      input: [file_hexdump, ppm_src],
      output: [dst],
      command: [prog_python, '@INPUT0@', 'binary', '@INPUT1@', '@OUTPUT@'],
    )
  endif

  executable(
    t,
    sources: [test_src, test_incs],
    dependencies: [idep_dxgiutil, idep_d3d11util],
  )
endforeach
