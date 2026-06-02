# Copyright 2024 Google LLC
# SPDX-License-Identifier: MIT

android_tests = [
  'ahbinfo',
]

drm_tests = [
  'drmdumb',
  'drminfo',
]

gbm_tests = [
  'gbminfo',
]

v4l2_tests = [
  'v4l2info',
]

if idep_androidutil.found()
  foreach t : android_tests
    executable(
      t,
      sources: [t + '.c'],
      dependencies: [idep_androidutil],
    )
  endforeach
endif

if idep_drmutil.found()
  foreach t : drm_tests
    executable(
      t,
      sources: [t + '.c'],
      dependencies: [idep_drmutil],
    )
  endforeach
endif

if idep_gbmutil.found()
  foreach t : gbm_tests
    executable(
      t,
      sources: [t + '.c'],
      dependencies: [idep_gbmutil],
    )
  endforeach
endif

if idep_v4l2util.found()
  foreach t : v4l2_tests
    executable(
      t,
      sources: [t + '.c'],
      dependencies: [idep_v4l2util],
    )
  endforeach
endif
