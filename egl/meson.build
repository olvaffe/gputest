# Copyright 2022 Google LLC
# SPDX-License-Identifier: MIT

idep_eglutil = declare_dependency(
  sources: ['eglutil.h'],
  dependencies: [dep_dl, dep_m, dep_gbm, dep_nativewindow],
  include_directories: [inc_include],
)

tests = [
  'clear',
  'fbo',
  'formats',
  'image',
  'info',
  'multithread',
  'tex',
  'timestamp',
  'tri',
]

if dep_sdl2.found()
  tests += ['sdl']
endif

foreach t : tests
  test_incs = []

  foreach a : ['t:vert', 't:tesc', 't:tese', 't:geom', 't:frag', 't:comp', 'b:ppm']
    asset = a.split(':')
    mode = asset[0]
    suffix = asset[1]

    src = t + '.' + suffix
    dst = t + '_test.' + suffix + '.inc'
    if fs.exists(src)
      test_incs += custom_target(
        dst,
        input: ['hexdump.py', src],
        output: [dst],
        command: [prog_python, '@INPUT0@', mode, '@INPUT1@', '@OUTPUT@'],
      )
    endif
  endforeach

  test_deps = [idep_eglutil]
  if t == 'sdl'
    test_deps += [dep_sdl2]
  endif

  executable(
    t,
    sources: [t + '.c', test_incs],
    dependencies: test_deps,
  )
endforeach
