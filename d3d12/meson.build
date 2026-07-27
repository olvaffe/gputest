# Copyright 2026 Google LLC
# SPDX-License-Identifier: MIT

d3d12_tests = [
  'info',
  'tri',
  'tex',
]

foreach t : d3d12_tests
  test_src = t + '.c'
  test_incs = []

  hlsl_src = t + '.hlsl'
  if fs.exists(hlsl_src)
    test_incs += custom_target(
      t + '_vs.h',
      input: [hlsl_src],
      output: [t + '_vs.h'],
      command: [prog_dxc, '-T', 'vs_6_0', '-E', 'vs_main', '-Vn', t + '_test_vs', '-Fh', '@OUTPUT@', '@INPUT@'],
    )
    test_incs += custom_target(
      t + '_ps.h',
      input: [hlsl_src],
      output: [t + '_ps.h'],
      command: [prog_dxc, '-T', 'ps_6_0', '-E', 'ps_main', '-Vn', t + '_test_ps', '-Fh', '@OUTPUT@', '@INPUT@'],
    )
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
    dependencies: [idep_d3d12util],
  )
endforeach
