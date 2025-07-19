cmock = subproject('CMock')
unity_dep = cmock.get_variable('unity_dep')
cmock_dep = cmock.get_variable('cmock_dep')

curl_include = libcurl_dependency.get_variable(pkgconfig: 'includedir') / 'curl'

ruby_bin = find_program('ruby', version: ['>=2.7', '<3.0'])
mocks_dir = meson.project_source_root() / 'tests' / 'mocks'

curl_cmock_config_path = mocks_dir / 'curl' / 'curl_cmock_config.yaml'
curl_wrappers_cmock_config_path = mocks_dir / 'curl_wrappers_cmock_config.yaml'

curl_easy_mocks = run_command(
  meson.project_source_root() / 'scripts' / 'generate_mocks.sh',
  curl_cmock_config_path,
  curl_include / 'easy.h',
  mocks_dir / 'curl',
  ruby_bin,
  'curl',
  capture: true,
  check: true,
)

curl_wrapper_mocks = run_command(
  meson.project_source_root() / 'scripts' / 'generate_mocks.sh',
  curl_wrappers_cmock_config_path,
  meson.project_source_root() / 'src' / 'curl_wrappers.h',
  mocks_dir,
  ruby_bin,
  capture: true,
  check: true,
)

curl_easy_mock_sources = curl_easy_mocks.stdout().split('\n')[1].split(' ')
curl_wrappers_easy_mock_sources = curl_wrapper_mocks.stdout().split('\n')[1].split(' ')
mock_sources = curl_easy_mock_sources + curl_wrappers_easy_mock_sources
message('Mock sources to generate: ', mock_sources)

mocks = static_library(
  'mocks',
  mock_sources,
  include_directories: ['mocks', src_inc],
  dependencies: [cmock_dep, unity_dep, jansson_dependency],
)

fixtures_dep = declare_dependency(
  sources: ['fixtures.c'],
  link_with: [rofi_reddit_shared_lib],
  dependencies: [libcurl_dependency],
)

test_env = ['LC_ALL=C']

test_access_token_fetch_exec = executable(
  'rofi-reddit-unit',
  ['fixtures.c', 'test_reddit.c', '../src/reddit.c', '../src/memory.c'],
  dependencies: [unity_dep, fixtures_dep] + deps,
  link_with: [mocks],
  include_directories: ['mocks', src_inc, project_inc],
)

test(
  'test_access_token_fetch',
  test_access_token_fetch_exec,
  env: test_env,
  protocol: 'exitcode',
  workdir: meson.current_source_dir(),
)

executable(
  'rofi-reddit-test-main',
  ['test_main.c'],
  dependencies: deps,
  link_with: [rofi_reddit_shared_lib],
  include_directories: [src_inc],
)


