project('rofi-reddit', 'c', version: '0.1.0')

add_project_arguments('-DUNITY_OUTPUT_COLOR=1', language: 'c')

glib_dependency = dependency('glib-2.0', fallback: ['glib'])
rofi_dependency = dependency('rofi')
libcurl_dependency = dependency('libcurl', fallback: ['curl'], version: ['>=8.0', '<9.0'])
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

home_dir = run_command('sh', '-c', 'echo $HOME', capture: true, check: true).stdout().strip()
plugin_config_dir = home_dir / '.config' / 'rofi-reddit'
config_file = plugin_config_dir / 'config.toml'
install_data(
  'config.toml',
  install_dir: plugin_config_dir,
)

subdir('src')
subdir('tests')
