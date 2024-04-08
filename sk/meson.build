# Copyright 2023 Google LLC
# SPDX-License-Identifier: MIT

tests = [
  'canvas-null',
  'canvas-pdf',
  'canvas-picture',
  'canvas-raster',
  'canvas-svg',
  'drawable',
  'image-raster',
]

if idep_eglutil.found()
  tests += ['canvas-ganesh-gl']
endif
if idep_vkutil.found()
  tests += ['canvas-ganesh-vk', 'image-ganesh-vk']
endif

foreach t : tests
  test_deps = [idep_skutil]

  if t == 'canvas-ganesh-gl'
    test_deps += [idep_eglutil]
  elif t in ['canvas-ganesh-vk', 'image-ganesh-vk']
    test_deps += [idep_vkutil]
  endif

  executable(
    t,
    sources: t + '.cpp',
    dependencies: test_deps,
  )
endforeach
