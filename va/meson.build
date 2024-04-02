# Copyright 2022 Google LLC
# SPDX-License-Identifier: MIT

idep_vautil = declare_dependency(
  sources: ['vautil.h'],
  dependencies: [dep_dl, dep_m, dep_drm, dep_libva, dep_libva_drm],
)

tests = [
  'ffdec',
  'info',
  'jpegdec',
]

foreach t : tests
  test_deps = [idep_vautil]

  if t == 'ffdec'
    if dep_libavcodec.found() and dep_libavformat.found() and dep_libavutil.found()
      test_deps += [dep_libavcodec, dep_libavformat, dep_libavutil]
    else
      continue
    endif
  endif

  executable(
    t,
    sources: t + '.c',
    dependencies: test_deps,
  )
endforeach
