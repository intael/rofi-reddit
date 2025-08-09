cmock = subproject('CMock')
unity_dep = cmock.get_variable('unity_dep')
cmock_dep = cmock.get_variable('cmock_dep')

ruby_bin = find_program('ruby', version: ['>=2.7', '<3.0'])
mocks_dir = meson.project_source_root() / 'tests' / 'mocks'
message(mocks_dir)

curl_cmock_config_path = mocks_dir / 'curl' / 'curl_cmock_config.yaml'
curl_wrappers_cmock_config_path = mocks_dir / 'curl_wrappers_cmock_config.yaml'

generate_mocks_script = meson.project_source_root() / 'generate_mocks.sh'

curl_include = libcurl_dependency.get_variable(
  pkgconfig: 'includedir',
  default_value: meson.project_source_root() / 'subprojects' / 'curl-8.10.1' / 'include',
)
message('Curl include directory: ', curl_include)
curl_easy_mocks = run_command(
  generate_mocks_script,
  curl_cmock_config_path,
  curl_include / 'curl' / 'easy.h',
  mocks_dir / 'curl',
  ruby_bin,
  'curl',
  capture: true,
  check: true,
)

curl_wrapper_mocks = run_command(
  generate_mocks_script,
  curl_wrappers_cmock_config_path,
  meson.project_source_root() / 'src' / 'curl_wrappers.h',
  mocks_dir,
  ruby_bin,
  capture: true,
  check: true,
)

curl_easy_mock_sources = curl_easy_mocks.stdout().split('\n')[1].split(' ')
curl_wrappers_easy_mock_sources = curl_wrapper_mocks.stdout().split('\n')[1].split(' ')
mock_sources = []
foreach x : curl_easy_mock_sources + curl_wrappers_easy_mock_sources
  if x != ''
    mock_sources += x
  endif
endforeach
message('Mock sources to build: ', mock_sources)

mocks = static_library(
  'mocks',
  mock_sources,
  include_directories: ['mocks', project_inc],
  dependencies: [cmock_dep, unity_dep, jansson_dependency, libcurl_dependency],
)

test_env = ['LC_ALL=C']

unit_test_access_token_fetch_exec = executable(
  'unit-test-access-token',
  ['fixtures.c', 'test_access_token_fetch.c'],
  objects: rofi_reddit_shared_lib.extract_objects('reddit.c', 'memory.c'),
  dependencies: [unity_dep] + deps,
  link_with: [mocks],
  include_directories: ['mocks', project_inc],
)

test(
  'unit_test_access_token',
  unit_test_access_token_fetch_exec,
  env: test_env,
  protocol: 'exitcode',
  workdir: meson.current_source_dir(),
)

message('Expected config file path: ', config_file)

if fs.exists(config_file)
  integration_test_access_token_exec = executable(
    'integration-test-access-token',
    ['integration_test_access_token.c'],
    objects: rofi_reddit_shared_lib.extract_objects('reddit.c', 'curl_wrappers.c', 'memory.c'),
    dependencies: deps + [unity_dep],
    include_directories: [project_inc],
  )

  test(
    'integration_test_access_token',
    integration_test_access_token_exec,
    env: test_env,
    protocol: 'exitcode',
    workdir: meson.current_source_dir(),
  )
else
  message(
    'Skipping integration_test_access_token: config file not found at '
    + config_file,
  )
endif
