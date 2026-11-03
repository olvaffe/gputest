# Copyright 2026 Google LLC
# SPDX-License-Identifier: MIT

mtl_tests = [
  'info',
  'tex',
  'tri',
]

foreach t : mtl_tests
  test_src = t + '.cpp'
  test_incs = []

  metal_src = t + '.metal'
  if fs.exists(metal_src)
    dst = t + '_test.metal.inc'
    test_incs += custom_target(
      dst,
      input: [file_hexdump, metal_src],
      output: [dst],
      command: [prog_python, '@INPUT0@', 'text', '@INPUT1@', '@OUTPUT@'],
    )
  endif

  executable(
    t,
    sources: [test_src, test_incs],
    dependencies: [idep_mtlutil],
  )
endforeach
