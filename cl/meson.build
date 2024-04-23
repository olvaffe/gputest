# Copyright 2022 Google LLC
# SPDX-License-Identifier: MIT

cl_tests = [
  'bench_arith',
  'bench_copy',
  'bench_fill',
  'clinfo',
  'copy',
  'tflite_bhwc_to_tensor',
  'tflite_conv_generic',
  'tflite_conv_simple',
  'tflite_depthwise_conv',
]

foreach t : cl_tests
  test_incs = []
  asset = t + '.cl'
  if fs.exists(asset)
    inc = t + '_test.cl.inc'
    test_incs += custom_target(
      inc,
      input: [file_hexdump, asset],
      output: [inc],
      command: [prog_python, '@INPUT0@', 'text', '@INPUT1@', '@OUTPUT@'],
    )
  endif

  executable(
    t,
    sources: [t + '.c', test_incs],
    dependencies: [idep_clutil],
  )
endforeach
