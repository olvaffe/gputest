# Copyright 2023 Google LLC
# SPDX-License-Identifier: MIT

tests = [
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

if idep_eglutil.found()
  tests += ['canvas-ganesh-gl']
endif

foreach t : tests
  test_deps = [idep_skutil]

  if t == 'canvas-ganesh-gl'
    test_deps += [idep_eglutil]
  endif

  executable(
    t,
    sources: t + '.cpp',
    dependencies: test_deps,
  )
endforeach
