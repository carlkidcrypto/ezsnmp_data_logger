project(
  'ezsnmp_data_logger',
  ['c', 'cpp'],
  version: '2026.11.1a0',
  license: 'MIT',
  default_options: [
    'cpp_std=c++17',
    'warning_level=2',
  ]
)

fs = import('fs')
pkg = import('pkgconfig')
cxx = meson.get_compiler('cpp')

thread_dep = dependency('threads')

# Windows Winsock2 support for PrometheusSink HTTP server
socket_deps = []
if host_machine.system() == 'windows'
  socket_deps += cxx.find_library('ws2_32', required: true)
endif

# Locate sibling or subproject ezsnmp C++17 sources
ezsnmp_opt = get_option('ezsnmp')
ezsnmp_root = get_option('ezsnmp_source_dir')
if ezsnmp_root == ''
  if fs.is_dir(join_paths(meson.current_source_dir(), '..', 'ezsnmp', 'ezsnmp', 'include'))
    ezsnmp_root = join_paths('..', 'ezsnmp')
  elif fs.is_dir(join_paths(meson.current_source_dir(), 'subprojects', 'ezsnmp', 'ezsnmp', 'include'))
    ezsnmp_root = join_paths('subprojects', 'ezsnmp')
  endif
endif

netsnmp_dep = dependency('netsnmp', required: ezsnmp_opt)
openssl_dep = declare_dependency()
if host_machine.system() == 'darwin' and netsnmp_dep.found()
  openssl_dep = dependency('openssl', required: ezsnmp_opt)
endif

have_ezsnmp = false
ezsnmp_link_libs = []
ezsnmp_inc_dirs = []
ezsnmp_deps = []

if not ezsnmp_opt.disabled() and netsnmp_dep.found() and ezsnmp_root != ''
  ezsnmp_inc_path = join_paths(ezsnmp_root, 'ezsnmp', 'include')
  ezsnmp_src_path = join_paths(ezsnmp_root, 'ezsnmp', 'src')
  ezsnmp_patched_path = join_paths(ezsnmp_src_path, 'net-snmp-5.10-final-patched')

  if fs.is_dir(join_paths(meson.current_source_dir(), ezsnmp_inc_path))
    have_ezsnmp = true
    ezsnmp_inc_dirs = include_directories(ezsnmp_root, ezsnmp_inc_path)
    ezsnmp_deps = [netsnmp_dep, openssl_dep]

    ezsnmp_core_lib = static_library(
      'ezsnmp_session_core',
      [
        join_paths(ezsnmp_src_path, 'datatypes.cpp'),
        join_paths(ezsnmp_src_path, 'exceptionsbase.cpp'),
        join_paths(ezsnmp_src_path, 'helpers.cpp'),
        join_paths(ezsnmp_src_path, 'thread_safety.cpp'),
        join_paths(ezsnmp_src_path, 'sessionbase.cpp'),
        join_paths(ezsnmp_patched_path, 'snmpbulkget.cpp'),
        join_paths(ezsnmp_patched_path, 'snmpbulkwalk.cpp'),
        join_paths(ezsnmp_patched_path, 'snmpget.cpp'),
        join_paths(ezsnmp_patched_path, 'snmpgetnext.cpp'),
        join_paths(ezsnmp_patched_path, 'snmpset.cpp'),
        join_paths(ezsnmp_patched_path, 'snmpwalk.cpp'),
      ],
      include_directories: ezsnmp_inc_dirs,
      dependencies: [thread_dep, netsnmp_dep, openssl_dep],
      pic: true,
    )
    ezsnmp_link_libs = [ezsnmp_core_lib]
    message('Linking ezsnmp C++17 SessionBase layer from: ' + ezsnmp_root)
  elif ezsnmp_opt.enabled()
    error('ezsnmp feature enabled, but ezsnmp C++ headers not found at: ' + ezsnmp_inc_path)
  endif
elif ezsnmp_opt.enabled()
  error('ezsnmp feature enabled, but netsnmp or ezsnmp_source_dir was not found')
endif

# SQLite3 dependency: prefer self-contained third_party/sqlite3 amalgamation
embedded_sqlite3 = false
sqlite_link_libs = []
sqlite_inc_dirs = []
sqlite_dep = declare_dependency()

if fs.is_file(join_paths(meson.current_source_dir(), 'third_party', 'sqlite3', 'sqlite3.c'))
  embedded_sqlite3 = true
  have_sqlite3 = true
  sqlite_inc_dirs = [include_directories('third_party/sqlite3')]
  sqlite_c_args = [
    '-DSQLITE_THREADSAFE=1',
    '-DSQLITE_ENABLE_FTS5=1',
    '-DSQLITE_ENABLE_JSON1=1',
    '-DSQLITE_OMIT_LOAD_EXTENSION=1',
  ]
  if cxx.get_id() != 'msvc'
    sqlite_c_args += ['-w']
  endif
  sqlite_embedded_lib = static_library(
    'ezsnmp_dl_sqlite3_embedded',
    'third_party/sqlite3/sqlite3.c',
    include_directories: sqlite_inc_dirs,
    c_args: sqlite_c_args,
    pic: true,
  )
  sqlite_link_libs = [sqlite_embedded_lib]
  message('Embedding self-contained SQLite3 amalgamation from third_party/sqlite3/sqlite3.c')
else
  sqlite_dep = dependency('sqlite3', required: get_option('sqlite3'))
  have_sqlite3 = sqlite_dep.found()
endif

# Optional PostgreSQL (libpq) dependency
pq_dep = dependency('libpq', required: get_option('psql'))
have_libpq = pq_dep.found()

dl_compile_args = [
  '-DEZSNMP_DL_HAVE_EZSNMP=' + (have_ezsnmp ? '1' : '0'),
  '-DEZSNMP_DL_HAVE_SQLITE3=' + (have_sqlite3 ? '1' : '0'),
  '-DEZSNMP_DL_EMBEDDED_SQLITE3=' + (embedded_sqlite3 ? '1' : '0'),
  '-DEZSNMP_DL_HAVE_LIBPQ=' + (have_libpq ? '1' : '0'),
]

public_inc = include_directories('include')
all_inc = [public_inc] + sqlite_inc_dirs
if have_ezsnmp
  all_inc += [ezsnmp_inc_dirs]
endif

all_deps = [thread_dep] + socket_deps + ezsnmp_deps
if have_sqlite3 and not embedded_sqlite3
  all_deps += [sqlite_dep]
endif
if have_libpq
  all_deps += [pq_dep]
endif

core_sources = files(
  'src/version.cpp',
  'src/json_utils.cpp',
  'src/datatypes.cpp',
  'src/config.cpp',
  'src/snmp_poller.cpp',
  'src/csv_sink.cpp',
  'src/jsonl_sink.cpp',
  'src/sqlite_sink.cpp',
  'src/psql_sink.cpp',
  'src/prometheus_sink.cpp',
  'src/sink_inspector.cpp',
  'src/data_logger.cpp',
  'src/cli_runner.cpp',
)

ezsnmp_dl_lib = both_libraries(
  'ezsnmp_data_logger',
  core_sources,
  include_directories: all_inc,
  cpp_args: dl_compile_args,
  link_whole: ezsnmp_link_libs + sqlite_link_libs,
  dependencies: all_deps,
  install: true,
)

ezsnmp_dl_dep = declare_dependency(
  link_with: ezsnmp_dl_lib.get_static_lib(),
  include_directories: all_inc,
  compile_args: dl_compile_args,
  dependencies: all_deps,
)

install_subdir('include/ezsnmp_data_logger', install_dir: get_option('includedir'))

pkg.generate(
  ezsnmp_dl_lib.get_shared_lib(),
  name: 'ezsnmp_data_logger',
  description: 'C++17 SNMP Data Logger powered by ezsnmp with multi-language SWIG bindings',
  version: meson.project_version(),
)

executable(
  'ezsnmp-data-logger',
  'src/main.cpp',
  dependencies: [ezsnmp_dl_dep],
  install: true,
)

# Backward-compatible binary alias for legacy daemon scripts
executable(
  'ezsnmp_loggerd',
  'src/main.cpp',
  dependencies: [ezsnmp_dl_dep],
  install: true,
)

if get_option('tests')
  subdir('cpp_tests')
endif
