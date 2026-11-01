cmake_minimum_required(VERSION 3.16)
project(ezsnmp_data_logger VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(EZSNMP_DL_BUILD_TESTS "Build GoogleTest suite" OFF)
set(EZSNMP_SOURCE_DIR "" CACHE PATH "Path to ezsnmp repository root")

find_package(Threads REQUIRED)
find_package(SQLite3 QUIET)
find_package(PostgreSQL QUIET)
find_package(PkgConfig QUIET)

if(EZSNMP_SOURCE_DIR STREQUAL "")
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../ezsnmp/ezsnmp/include/sessionbase.h")
    set(EZSNMP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ezsnmp")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/subprojects/ezsnmp/ezsnmp/include/sessionbase.h")
    set(EZSNMP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/subprojects/ezsnmp")
  endif()
endif()

set(HAVE_EZSNMP 0)
set(EZSNMP_EXTRA_LIBS "")
set(EZSNMP_EXTRA_INCS "")

if(PkgConfig_FOUND AND NOT EZSNMP_SOURCE_DIR STREQUAL "")
  pkg_check_modules(NETSNMP netsnmp)
  if(NETSNMP_FOUND)
    set(HAVE_EZSNMP 1)
    if(APPLE)
      find_package(OpenSSL REQUIRED)
      list(APPEND EZSNMP_EXTRA_LIBS OpenSSL::Crypto OpenSSL::SSL)
    endif()
    add_library(ezsnmp_session_core STATIC
      "${EZSNMP_SOURCE_DIR}/ezsnmp/src/datatypes.cpp"
      "${EZSNMP_SOURCE_DIR}/ezsnmp/src/exceptionsbase.cpp"
      "${EZSNMP_SOURCE_DIR}/ezsnmp/src/helpers.cpp"
      "${EZSNMP_SOURCE_DIR}/ezsnmp/src/thread_safety.cpp"
      "${EZSNMP_SOURCE_DIR}/ezsnmp/src/sessionbase.cpp"
      "${EZSNMP_SOURCE_DIR}/ezsnmp/src/net-snmp-5.10-final-patched/snmpbulkget.cpp"
      "${EZSNMP_SOURCE_DIR}/ezsnmp/src/net-snmp-5.10-final-patched/snmpbulkwalk.cpp"
      "${EZSNMP_SOURCE_DIR}/ezsnmp/src/net-snmp-5.10-final-patched/snmpget.cpp"
      "${EZSNMP_SOURCE_DIR}/ezsnmp/src/net-snmp-5.10-final-patched/snmpgetnext.cpp"
      "${EZSNMP_SOURCE_DIR}/ezsnmp/src/net-snmp-5.10-final-patched/snmpset.cpp"
      "${EZSNMP_SOURCE_DIR}/ezsnmp/src/net-snmp-5.10-final-patched/snmpwalk.cpp"
    )
    target_include_directories(ezsnmp_session_core PUBLIC
      "${EZSNMP_SOURCE_DIR}"
      "${EZSNMP_SOURCE_DIR}/ezsnmp/include"
      ${NETSNMP_INCLUDE_DIRS}
    )
    target_link_directories(ezsnmp_session_core PUBLIC ${NETSNMP_LIBRARY_DIRS})
    target_link_libraries(ezsnmp_session_core PUBLIC ${NETSNMP_LIBRARIES} Threads::Threads ${EZSNMP_EXTRA_LIBS})
  endif()
endif()

add_library(ezsnmp_data_logger
  src/version.cpp
  src/json_utils.cpp
  src/datatypes.cpp
  src/config.cpp
  src/snmp_poller.cpp
  src/csv_sink.cpp
  src/jsonl_sink.cpp
  src/sqlite_sink.cpp
  src/psql_sink.cpp
  src/prometheus_sink.cpp
  src/sink_inspector.cpp
  src/data_logger.cpp
  src/cli_runner.cpp
)

target_include_directories(ezsnmp_data_logger PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_definitions(ezsnmp_data_logger PUBLIC
  EZSNMP_DL_HAVE_EZSNMP=${HAVE_EZSNMP}
  EZSNMP_DL_HAVE_SQLITE3=$<BOOL:${SQLite3_FOUND}>
  EZSNMP_DL_HAVE_LIBPQ=$<BOOL:${PostgreSQL_FOUND}>
)

target_link_libraries(ezsnmp_data_logger PUBLIC Threads::Threads)
if(WIN32)
  target_link_libraries(ezsnmp_data_logger PUBLIC ws2_32)
endif()
if(HAVE_EZSNMP)
  target_link_libraries(ezsnmp_data_logger PUBLIC ezsnmp_session_core)
endif()
if(SQLite3_FOUND)
  target_link_libraries(ezsnmp_data_logger PUBLIC SQLite::SQLite3)
endif()
if(PostgreSQL_FOUND)
  target_link_libraries(ezsnmp_data_logger PUBLIC PostgreSQL::PostgreSQL)
endif()

add_executable(ezsnmp-data-logger src/main.cpp)
target_link_libraries(ezsnmp-data-logger PRIVATE ezsnmp_data_logger)
