# Copyright 2022 Google LLC
# SPDX-License-Identifier: MIT

tests = [
  'clear',
  'fence',
  'formats',
  'info',
  'ssbo',
  'tex',
  'timestamp',
  'tri',
]

if idep_androidutil.found() or idep_gbmutil.found()
  tests += ['image']
endif
if idep_androidutil.found()
  tests += ['ahb_ssbo']
endif
if idep_gbmutil.found()
  tests += ['multithread']
endif
if idep_sdlutil.found()
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
        input: [file_hexdump, src],
        output: [dst],
        command: [prog_python, '@INPUT0@', mode, '@INPUT1@', '@OUTPUT@'],
      )
    endif
  endforeach

  test_deps = [idep_eglutil]
  if t == 'ahb_ssbo'
    test_deps += [idep_androidutil]
  elif t == 'image'
    test_deps += [idep_androidutil, idep_gbmutil]
  elif t == 'multithread'
    test_deps += [idep_gbmutil]
  elif t == 'sdl'
    test_deps += [idep_sdlutil]
  endif

  executable(
    t,
    sources: [t + '.c', test_incs],
    dependencies: test_deps,
  )
endforeach
