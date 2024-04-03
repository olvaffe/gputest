# Copyright 2022 Google LLC
# SPDX-License-Identifier: MIT

tests = [
  'info',
  'jpegdec',
]

if idep_ffutil.found()
  tests += ['ffdec']
endif

if idep_v4l2util.found()
  tests += ['v4l2info']
endif

foreach t : tests
  test_deps = [idep_vautil]

  if t == 'ffdec'
    test_deps += [idep_ffutil]
  elif t == 'v4l2info'
    test_deps += [idep_v4l2util]
  endif

  executable(
    t,
    sources: t + '.c',
    dependencies: test_deps,
  )
endforeach
