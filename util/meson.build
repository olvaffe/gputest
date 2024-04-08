# Copyright 2024 Google LLC
# SPDX-License-Identifier: MIT

file_hexdump = files('hexdump.py')

util_summary = {}

idep_util = declare_dependency(
  sources: ['util.h'],
  dependencies: [dep_m, dep_rt],
  include_directories: ['.', inc_include],
)

idep_androidutil = dep_not_found
if cc.has_header('android/hardware_buffer.h')
  idep_androidutil = declare_dependency(
    sources: ['androidutil.h'],
    dependencies: [idep_util],
  )
endif
util_summary += {'androidutil': idep_androidutil}

idep_clutil = dep_not_found
if cc.has_header('CL/cl.h')
  idep_clutil = declare_dependency(
    sources: ['clutil.h'],
    dependencies: [idep_util, dep_dl],
  )
endif
util_summary += {'clutil': idep_clutil}

idep_drmutil = dep_not_found
if dep_drm.found()
  idep_drmutil = declare_dependency(
    sources: ['drmutil.h'],
    dependencies: [idep_util, dep_drm],
  )
endif
util_summary += {'drmutil': idep_drmutil}

idep_eglutil = dep_not_found
if dep_gbm.found() or dep_nativewindow.found()
  idep_eglutil = declare_dependency(
    sources: ['eglutil.h'],
    dependencies: [idep_util, dep_dl, dep_gbm, dep_nativewindow],
  )
endif
util_summary += {'eglutil': idep_eglutil}

idep_ffutil = dep_not_found
if dep_libavcodec.found() and dep_libavformat.found() and dep_libavutil.found()
  idep_ffutil = declare_dependency(
    sources: ['ffutil.h'],
    dependencies: [idep_util, dep_libavcodec, dep_libavformat, dep_libavutil],
  )
endif
util_summary += {'ffutil': idep_ffutil}

idep_gbmutil = dep_not_found
if dep_gbm.found()
  idep_gbmutil = declare_dependency(
    sources: ['gbmutil.h'],
    dependencies: [idep_util, dep_gbm],
  )
endif
util_summary += {'gbmutil': idep_gbmutil}

idep_sdlutil = dep_not_found
if dep_sdl2.found()
  idep_sdlutil = declare_dependency(
    sources: ['sdlutil.h'],
    dependencies: [idep_util, dep_sdl2],
  )
endif
util_summary += {'sdlutil': idep_sdlutil}

idep_skutil = dep_not_found
if dep_skia.found()
  idep_skutil = declare_dependency(
    sources: ['skutil.h', 'skutil_vk.h'],
    compile_args: ['-L' + skia_path / 'out', '-fno-rtti',
                   '-DSK_DEBUG', '-DSK_GANESH', '-DSK_GL', '-DSK_VULKAN'],
    dependencies: [idep_util, dep_dl, dep_skia, dep_egl, dep_glesv2],
    include_directories: [skia_path],
  )
endif
util_summary += {'skutil': idep_skutil}

idep_v4l2util = dep_not_found
if cc.has_header('linux/videodev2.h')
  idep_v4l2util = declare_dependency(
    sources: ['v4l2util.h'],
    dependencies: [idep_util],
  )
endif
util_summary += {'v4l2util': idep_v4l2util}

idep_vautil = dep_not_found
if dep_drm.found() and dep_libva.found() and dep_libva_drm.found()
  idep_vautil = declare_dependency(
    sources: ['vautil.h'],
    dependencies: [idep_util, dep_drm, dep_libva, dep_libva_drm],
  )
endif
util_summary += {'vautil': idep_vautil}

idep_vkutil = dep_not_found
if cc.has_header('vulkan/vulkan.h')
  idep_vkutil = declare_dependency(
    sources: ['vkutil.h'],
    dependencies: [idep_util, dep_dl],
  )
endif
util_summary += {'vkutil': idep_vkutil}

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
    dependencies: [idep_util, dep_wl_client],
  )
endif
util_summary += {'wlutil': idep_wlutil}
