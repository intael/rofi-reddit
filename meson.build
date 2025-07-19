project('rofi-reddit', 'c', version: '0.1.0')

add_project_arguments('-DUNITY_OUTPUT_COLOR=1', language: 'c')

glib_dependency = dependency('glib-2.0', fallback: ['glib'])
rofi_dependency = dependency('rofi', allow_fallback: true, version: ['>=1.7', '<2.0'])
libcurl_dependency = dependency('libcurl', fallback: ['curl'], version: ['>=8.0', '<9.0'])
jansson_dependency = dependency('jansson', allow_fallback: true, version: ['>=2.4', '<3.0'])
tomlc17_dependency = subproject('tomlc17').get_variable('tomlc17_dep')

deps = [
  glib_dependency,
  tomlc17_dependency,
  jansson_dependency,
  libcurl_dependency,
  rofi_dependency
]

main_sources = [
  'src/reddit.c',
  'src/curl_wrappers.c',
  'src/memory.c',
  'src/rofi_reddit.c',
]

pluginsdir = rofi_dependency.get_variable('pluginsdir')

link_args = []
if host_machine.system() == 'darwin'
  link_args = ['-Wl,-undefined,dynamic_lookup']
elif host_machine.system() == 'linux'
  link_args = ['-Wl,--unresolved-symbols=ignore-all']
endif

rofi_reddit_shared_lib = shared_library(
  meson.project_name(),
  main_sources,
  dependencies: deps,
  install_dir: pluginsdir, 
  name_prefix: '',
  install: true,
  link_args: link_args
)

project_inc = include_directories('.')
src_inc = include_directories('src')

subdir('tests')
