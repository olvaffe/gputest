# Copyright 2024 Google LLC
# SPDX-License-Identifier: MIT

drm_tests = [
  'drminfo',
]

if idep_drmutil.found()
  foreach t : drm_tests
    executable(
      t,
      sources: [t + '.c'],
      dependencies: [idep_drmutil],
    )
  endforeach
endif
