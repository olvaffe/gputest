# Copyright 2023 Google LLC
# SPDX-License-Identifier: MIT

idep_skutil = declare_dependency(
  sources: ['skutil.h'],
  dependencies: [dep_dl, dep_m, dep_skia],
)

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
