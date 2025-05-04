project(
  'sway-resize',
  'c',
  meson_version: '>=1.1',
  default_options: ['c_std=c11'],
  license: 'GPL-3.0',
  version: '0.1.0',
)

add_project_arguments('-D_GNU_SOURCE=200809L', language: 'c')
add_project_arguments('-DVERSION="@0@"'.format(meson.project_version()), language: 'c')
cc = meson.get_compiler('c')

wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols')
xkbcommon = dependency('xkbcommon')
cairo = dependency('cairo')
jansson = dependency('jansson')
math = cc.find_library('m')

subdir('protocol')

executable(
  'sway-resize',
  [
    'src/main.c',
    'src/surface_buffer.c',
    'src/utils_cairo.c',
    'src/utils.c',
    'src/utils_cairo.c',
    'src/sway_ipc.c',
    'src/sway_win.c',
    'src/resize_params.c',
    'src/render.c',
    protos_src,
  ],
  dependencies: [
    wayland_client,
    xkbcommon,
    cairo,
    math,
    wayland_client,
    jansson,
  ],
  install: true,
)

test(
  'test_resize_params',
  executable(
    'test_resize_params',
    [
      'src/test_resize_params.c',
      'src/resize_params.c',
      'src/utils.c',
    ],
  ),
)
