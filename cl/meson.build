# Copyright 2022 Google LLC
# SPDX-License-Identifier: MIT

cl_tests = [
  'bench_arith',
  'bench_copy',
  'bench_fill',
  'clinfo',
  'copy',
  'tflite_bhwc_to_tensor',
  'tflite_depthwise_conv',
]

foreach t : cl_tests
  executable(
    t,
    sources: [t + '.c'],
    dependencies: [idep_clutil],
  )
endforeach
