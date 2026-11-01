gtest_dep = dependency('gtest', main: true, required: true)

test_env = environment()
test_env.set('MIBS', 'ALL')
mibdirs_cmd = run_command('net-snmp-config', '--default-mibdirs', check: false)
if mibdirs_cmd.returncode() == 0 and mibdirs_cmd.stdout().strip() != ''
  test_env.set('MIBDIRS', mibdirs_cmd.stdout().strip())
endif

test_files = {
  'json_utils': 'test_json_utils.cpp',
  'config_and_datatypes': 'test_config_and_datatypes.cpp',
  'queue_and_sinks': 'test_queue_and_sinks.cpp',
  'ezsnmp_poller': 'test_ezsnmp_poller.cpp',
  'data_logger_and_cli': 'test_data_logger_and_cli.cpp',
}

foreach name, src : test_files
  exe = executable(
    'test_' + name,
    src,
    dependencies: [ezsnmp_dl_dep, gtest_dep],
  )
  test(name + '_test', exe, env: test_env, timeout: 60)
endforeach
