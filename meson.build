project('ezsnmp_data_logger', 'cpp',
  version : '0.1.0',
  default_options : ['cpp_std=c++17', 'warning_level=2'])

# Dependencies
zmq_dep = dependency('libzmq', required: true)
sqlite_dep = dependency('sqlite3', required: true)
libpq_dep = dependency('libpq', required: true)
nlohmann_json_dep = dependency('nlohmann_json', required: true)

# msgpack is header-only, find it manually
msgpack_inc = include_directories('/usr/include')

# Daemon sources
daemon_sources = [
  'daemon/config.cc',
  'daemon/batch.cc',
  'daemon/csv_sink.cc',
  'daemon/sqlite_sink.cc',
  'daemon/psql_sink.cc',
  'daemon/daemon.cc',
  'daemon/main.cc',
]

executable('ezsnmp_loggerd',
  daemon_sources,
  include_directories: [msgpack_inc, include_directories('daemon')],
  dependencies: [zmq_dep, sqlite_dep, libpq_dep, nlohmann_json_dep],
  install: true)
