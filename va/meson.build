# Copyright 2022 Google LLC
# SPDX-License-Identifier: MIT

idep_vautil = declare_dependency(
  sources: ['vautil.h'],
  dependencies: [dep_dl, dep_m, dep_drm, dep_libva, dep_libva_drm],
)

tests = [
  'info',
  'jpegdec',
]

if idep_ffutil.found()
  tests += ['ffdec']
endif

foreach t : tests
  test_deps = [idep_vautil]

  if t == 'ffdec'
    test_deps += [idep_ffutil]
  endif

  executable(
    t,
    sources: t + '.c',
    dependencies: test_deps,
  )
endforeach
