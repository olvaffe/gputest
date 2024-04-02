# Copyright 2024 Google LLC
# SPDX-License-Identifier: MIT

idep_drmutil = dep_not_found
if dep_drm.found()
  idep_drmutil = declare_dependency(
    sources: ['drmutil.h'],
    dependencies: [dep_drm],
    include_directories: ['.', inc_include],
  )
endif

idep_eglutil = dep_not_found
if dep_gbm.found() or dep_nativewindow.found()
  idep_eglutil = declare_dependency(
    sources: ['eglutil.h'],
    dependencies: [dep_dl, dep_m, dep_gbm, dep_nativewindow],
    include_directories: ['.', inc_include],
  )
endif

idep_ffutil = dep_not_found
if dep_libavcodec.found() and dep_libavformat.found() and dep_libavutil.found()
  idep_ffutil = declare_dependency(
    sources: ['ffutil.h'],
    dependencies: [dep_libavcodec, dep_libavformat, dep_libavutil],
    include_directories: ['.', inc_include],
  )
endif

idep_skutil = dep_not_found
if dep_skia.found()
  idep_skutil = declare_dependency(
    sources: ['skutil.h'],
    compile_args: ['-L' + skia_path / 'out', '-fno-rtti',
                   '-DSK_DEBUG', '-DSK_GANESH', '-DSK_GL', '-DSK_VULKAN'],
    dependencies: [dep_dl, dep_m, dep_skia],
    include_directories: ['.', skia_path],
  )
endif

idep_vautil = dep_not_found
if dep_drm.found() and dep_libva.found() and dep_libva_drm.found()
  idep_vautil = declare_dependency(
    sources: ['vautil.h'],
    dependencies: [dep_drm, dep_libva, dep_libva_drm],
    include_directories: ['.', inc_include],
  )
endif

idep_vkutil = declare_dependency(
  sources: ['vkutil.h'],
  dependencies: [dep_dl, dep_m, dep_rt],
  include_directories: ['.', inc_include],
)

idep_wlutil = dep_not_found
if dep_wl_client.found() and dep_wl_protocols.found() and dep_wl_scanner.found()
  protocols = [
    ['xdg-shell', 'stable/xdg-shell'],
    ['linux-dmabuf-unstable-v1', 'unstable/linux-dmabuf'],
  ]

  wl_scanner = find_program(dep_wl_scanner.get_variable(pkgconfig: 'wayland_scanner'))
  wl_protocols_dir = dep_wl_protocols.get_variable(pkgconfig: 'pkgdatadir')

  wl_protocols_files = []
  foreach proto : protocols
    xml_path = wl_protocols_dir / proto[1] / proto[0] + '.xml'

    wl_protocols_files += custom_target(proto[0] + ' client-header',
      command: [wl_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
      input: xml_path,
      output: proto[0] + '-protocol.h',
    )
    wl_protocols_files += custom_target(proto[0] + ' private-code',
      command: [wl_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
      input: xml_path,
      output: proto[0] + '-protocol.c',
    )
  endforeach

  idep_wlutil = declare_dependency(
    sources: ['wlutil.h', wl_protocols_files],
    dependencies: [dep_wl_client],
    include_directories: ['.', inc_include],
  )
endif
