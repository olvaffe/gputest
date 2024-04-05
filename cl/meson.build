# Copyright 2022 Google LLC
# SPDX-License-Identifier: MIT

cl_tests = [
  'clinfo',
]

foreach t : cl_tests
  executable(
    t,
    sources: [t + '.c'],
    dependencies: [idep_clutil],
  )
endforeach
