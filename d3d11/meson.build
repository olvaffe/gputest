# Copyright 2026 Google LLC
# SPDX-License-Identifier: MIT

d3d11_tests = [
  'info',
]

foreach t : d3d11_tests
  test_src = t + '.c'

  executable(
    t,
    sources: [test_src],
    dependencies: [idep_dxgiutil, idep_d3d11util],
  )
endforeach
