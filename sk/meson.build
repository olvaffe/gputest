# Copyright 2023 Google LLC
# SPDX-License-Identifier: MIT

tests = [
  'canvas-ganesh-gl',
  'canvas-ganesh-vk',
  'canvas-null',
  'canvas-pdf',
  'canvas-picture',
  'canvas-raster',
  'canvas-svg',
  'drawable',
  'image-ganesh-vk',
  'image-raster',
]

foreach t : tests
  test_deps = [idep_skutil]

  executable(
    t,
    sources: t + '.cpp',
    dependencies: test_deps,
  )
endforeach
