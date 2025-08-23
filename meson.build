project('rofi-reddit', 'c', version: '0.1.1', meson_version: '>=1.1')

add_project_arguments('-DUNITY_OUTPUT_COLOR=1', language: 'c')

glib_dependency = dependency('glib-2.0', fallback: ['glib'])
rofi_dependency = dependency('rofi')
libcurl_dependency = dependency('libcurl', allow_fallback: true, version: ['>=8.0', '<9.0'])
jansson_dependency = dependency('jansson', allow_fallback: true, version: ['>=2.4', '<3.0'])
tomlc17_dependency = subproject('tomlc17').get_variable('tomlc17_dep')

deps = [
  glib_dependency,
  tomlc17_dependency,
  jansson_dependency,
  libcurl_dependency,
  rofi_dependency,
]

project_inc = include_directories('src')

plugin_config_dir = '/' / 'usr' / 'share' / 'rofi-reddit'
config_file = plugin_config_dir / 'config.toml'

fs = import('fs')
if not fs.exists(config_file)
  install_data('config.toml', install_dir: plugin_config_dir, install_mode: 'rw----r--')
endif

subdir('src')
if get_option('tests')
  subdir('tests')
endif
