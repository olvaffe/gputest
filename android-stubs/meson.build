idep_android_stubs = dep_not_found
if not dep_android.found()
  subdir_done()
endif

libutils = shared_library(
  'utils',
  files('src/utils_stub.cpp'),
  include_directories : 'include',
)

libbinder = shared_library(
  'binder',
  files('src/binder_stub.cpp'),
  include_directories : 'include',
  link_with : libutils,
)

libgui = shared_library(
  'gui',
  files('src/gui_stub.cpp'),
  include_directories : 'include',
  link_with : libutils,
)

idep_android_stubs = declare_dependency(
  link_with : [libbinder, libgui, libutils],
  include_directories : 'include',
)
