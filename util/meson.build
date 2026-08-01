# Copyright 2024 Google LLC
# SPDX-License-Identifier: MIT

file_hexdump = files('hexdump.py')

util_summary = {}

idep_util = declare_dependency(
  sources: ['util.h'],
  dependencies: [dep_m, dep_rt, dep_log],
  include_directories: ['.', inc_include],
)

idep_androidutil = dep_not_found
if dep_nativewindow.found()
  idep_androidutil = declare_dependency(
    sources: ['androidutil.h'],
    dependencies: [idep_util, dep_nativewindow],
  )
endif
util_summary += {'androidutil': idep_androidutil}

idep_clutil = declare_dependency(
  sources: ['clutil.h'],
  dependencies: [idep_util, dep_dl],
)
util_summary += {'clutil': idep_clutil}

idep_dxgiutil = dep_not_found
if dep_dxvk_dxgi.found()
  idep_dxgiutil = declare_dependency(
    sources: ['dxgiutil.h'],
    dependencies: [idep_util, dep_dl, dep_dxvk_dxgi],
  )
endif
util_summary += {'dxgiutil': idep_dxgiutil}

idep_d3d11util = dep_not_found
if dep_dxvk_d3d11.found()
  idep_d3d11util = declare_dependency(
    sources: ['d3d11util.h'],
    dependencies: [idep_util, dep_dl, dep_dxvk_d3d11],
  )
endif
util_summary += {'d3d11util': idep_d3d11util}

idep_d3d12util = dep_not_found
if dep_vkd3d_proton.found()
  idep_d3d12util = declare_dependency(
    sources: ['d3d12util.h'],
    dependencies: [idep_util, dep_dl, dep_vkd3d_proton],
  )
endif
util_summary += {'d3d12util': idep_d3d12util}

idep_dmautil = declare_dependency(
  sources: ['dmautil.h'],
  dependencies: [idep_util],
)
util_summary += {'dmautil': idep_dmautil}

idep_drmutil = dep_not_found
if dep_drm.found()
  idep_drmutil = declare_dependency(
    sources: ['drmutil.h'],
    dependencies: [idep_util, dep_drm],
  )
endif
util_summary += {'drmutil': idep_drmutil}

idep_eglutil = declare_dependency(
  sources: ['eglutil.h'],
  dependencies: [idep_util, dep_dl],
)
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

idep_rdocutil = declare_dependency(
  sources: ['rdocutil.h'],
  dependencies: [idep_util, dep_dl],
)
util_summary += {'rdocutil': idep_rdocutil}

idep_sdlutil = dep_not_found
if dep_sdl3.found()
  idep_sdlutil = declare_dependency(
    sources: ['sdlutil.h'],
    dependencies: [idep_util, dep_sdl3],
  )
endif
util_summary += {'sdlutil': idep_sdlutil}

idep_skutil = dep_not_found
if dep_skia.found()
  idep_skutil = declare_dependency(
    sources: ['skutil.h', 'skutil_egl.h', 'skutil_vk.h'],
    compile_args: ['-L' + skia_path / 'out', '-fno-rtti',
                   '-DSK_DEBUG', '-DSK_GANESH', '-DSK_GL', '-DSK_VULKAN'],
    dependencies: [idep_util, dep_dl, dep_skia],
    include_directories: [skia_path],
  )
endif
util_summary += {'skutil': idep_skutil}

spvutil_deps = [idep_util]
spvutil_compile_args = []
spvutil_include_dirs = []
spvutil_link_args = []

spvutil_langs = ['.spv']
spvutil_tools = []

if dep_spirv_tools.found()
  spvutil_deps += [dep_spirv_tools]
  spvutil_compile_args += ['-DHAVE_SPIRV_TOOLS']
  spvutil_langs += ['.spvasm']
  spvutil_tools += ['disassembly']
endif

if dep_glslang.found()
  spvutil_deps += [dep_glslang]
  spvutil_compile_args += ['-DHAVE_GLSLANG']
  spvutil_link_args = ['-lglslang-default-resource-limits',
                       '-lMachineIndependent', '-lGenericCodeGen', '-lSPIRV']
  spvutil_langs += ['.vert/.frag/.comp/...']
endif

if dep_clspv.found()
  spvutil_deps += [dep_clspv]
  spvutil_compile_args += ['-DHAVE_CLSPV']
  spvutil_include_dirs += [clspv_path / 'include', spirv_reflect_path]
  spvutil_langs += ['.cl (for Vulkan)']
elif dep_llvm_spirv.found()
  spvutil_deps += [dep_dl, dep_llvm, dep_clang, dep_llvm_spirv]
  spvutil_compile_args += ['-DHAVE_LLVM_SPIRV']
  spvutil_langs += ['.cl (for OpenCL)']
endif

if dep_spirv_reflect.found()
  spvutil_deps += [dep_spirv_reflect]
  spvutil_compile_args += ['-DHAVE_SPIRV_REFLECT']
  spvutil_include_dirs += [spirv_reflect_path]
  spvutil_tools += ['reflection']
endif

idep_spvutil = declare_dependency(
  sources: ['spvutil.cpp', 'spvutil.h'],
  dependencies: spvutil_deps,
  compile_args: spvutil_compile_args,
  include_directories: spvutil_include_dirs,
  link_args: spvutil_link_args,
)
util_summary += {'spvutil': [
  idep_spvutil,
  'langs: ' + ', '.join(spvutil_langs),
  'tools: ' + ', '.join(spvutil_tools)
]}

idep_v4l2util = declare_dependency(
  sources: ['v4l2util.h'],
  dependencies: [idep_util],
)
util_summary += {'v4l2util': idep_v4l2util}

idep_vautil = dep_not_found
if dep_drm.found() and dep_libva.found() and dep_libva_drm.found()
  idep_vautil = declare_dependency(
    sources: ['vautil.h'],
    dependencies: [idep_util, dep_drm, dep_libva, dep_libva_drm],
  )
endif
util_summary += {'vautil': idep_vautil}

idep_vkutil = declare_dependency(
  sources: ['vkutil.h', 'vkutil_allocator.h', 'vkutil_profiles.hpp'],
  dependencies: [idep_util, dep_dl],
)
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
