project('rofi-reddit', 'c', version: '0.1.0')


add_project_arguments('-DUNITY_OUTPUT_COLOR=1', language: 'c')

glib_dependency = dependency('glib-2.0', fallback: ['glib'])
rofi_dependency = dependency('rofi', allow_fallback: true)
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


cmock = subproject('CMock')
unity_dep = cmock.get_variable('unity_dep')
cmock_dep = cmock.get_variable('cmock_dep')


curl_include = libcurl_dependency.get_variable(pkgconfig: 'includedir') / 'curl'

ruby_bin = find_program('ruby', version: ['>=2.7', '<3.0'])
mocks_dir = meson.project_source_root() / 'tests' / 'mocks'

curl_cmock_config_path = mocks_dir / 'curl' / 'curl_cmock_config.yaml'
curl_wrappers_cmock_config_path = mocks_dir / 'curl_wrappers_cmock_config.yaml'


curl_easy_mocks = run_command(
  'scripts' / 'generate_mocks.sh',
  curl_cmock_config_path,
  curl_include / 'easy.h',
  mocks_dir / 'curl',
  ruby_bin,
  'curl',
  capture: true,
  check: true
)

curl_wrapper_mocks = run_command(
  'scripts' / 'generate_mocks.sh',
  curl_wrappers_cmock_config_path,
  'src' / 'curl_wrappers.h',
  mocks_dir,
  ruby_bin,
  capture: true,
  check: true
)

curl_easy_mock_sources = curl_easy_mocks.stdout().split('\n')[1].split(' ')
curl_wrappers_easy_mock_sources = curl_wrapper_mocks.stdout().split('\n')[1].split(' ')
mock_sources = curl_easy_mock_sources + curl_wrappers_easy_mock_sources

mocks = static_library('mocks', 
  mock_sources,
  include_directories: ['tests/mocks','src'],
  dependencies: [cmock_dep, unity_dep, declare_dependency(sources: 'tests/fixtures.c')] + deps,
)


test_exe = executable('rofi-reddit-unit', 
  ['tests/test_reddit.c'],
  dependencies: deps + [unity_dep, declare_dependency(sources: ['src/reddit.c', 'src/memory.c'])],
  link_with: [mocks],
  include_directories: ['src', 'tests/mocks']
)

executable('rof-reddit-test-main',
  ['tests/test_main.c', 'src/reddit.c', 'src/memory.c', 'src/curl_wrappers.c'],
  dependencies: deps,
  include_directories: ['src']
)

test_env = ['LC_ALL=C']

test(
  'test-suite-unit',
  test_exe,
  env: test_env,
  protocol: 'exitcode',
  workdir: join_paths(meson.current_source_dir(), 'tests')
)

