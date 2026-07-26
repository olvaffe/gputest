# Copyright 2026 Google LLC
# SPDX-License-Identifier: MIT

d3d12_tests = [
  'info',
]

foreach t : d3d12_tests
  executable(
    t,
    sources: [t + '.c'],
    dependencies: [idep_d3d12util],
  )
endforeach
