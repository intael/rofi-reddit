main_sources = [
  'reddit.c',
  'curl_wrappers.c',
  'memory.c',
  'rofi_reddit.c',
]

pluginsdir = rofi_dependency.get_variable('pluginsdir')

link_args = []
if host_machine.system() == 'darwin'
  link_args = ['-Wl,-undefined,dynamic_lookup']
elif host_machine.system() == 'linux'
  link_args = ['-Wl,--unresolved-symbols=ignore-all']
endif

rofi_reddit_shared_lib = shared_module(
  'reddit',
  main_sources,
  dependencies: deps,
  install_dir: pluginsdir,
  name_prefix: '',
  install: true,
  link_args: link_args,
)
