# Copyright 2024 Google LLC
# SPDX-License-Identifier: MIT

idep_ffutil = dep_not_found
if dep_libavcodec.found() and dep_libavformat.found() and dep_libavutil.found()
  idep_ffutil = declare_dependency(
    sources: ['ffutil.h'],
    dependencies: [dep_libavcodec, dep_libavformat, dep_libavutil],
    include_directories: ['.', inc_include],
  )
endif

idep_vkutil = declare_dependency(
  sources: ['vkutil.h'],
  dependencies: [dep_dl, dep_m, dep_rt],
  include_directories: ['.', inc_include],
)
