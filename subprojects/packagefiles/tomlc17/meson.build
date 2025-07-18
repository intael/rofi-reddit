project('tomlc17', 'c')

tomlc17 = static_library(
  'tomlc17',
  'src/tomlc17.c',
  include_directories: include_directories('src')
)

tomlc17Dependency = declare_dependency(
  link_with: tomlc17,
  include_directories: include_directories('src')
)
