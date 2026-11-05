cmake_minimum_required(VERSION 3.16)
project(ezsnmp_data_logger VERSION 2026.11.6 LANGUAGES C CXX)

set(EZSNMP_DL_VERSION_STRING "2026.11.6")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC)
  add_compile_options(/MP)
endif()

option(EZSNMP_DL_BUILD_TESTS "Build GoogleTest suite" OFF)
set(EZSNMP_SOURCE_DIR "" CACHE PATH "Path to ezsnmp repository root")

find_package(Threads REQUIRED)
find_package(PostgreSQL QUIET)
find_package(PkgConfig QUIET)

set(EMBEDDED_SQLITE3 0)
set(HAVE_SQLITE3 0)
set(SQLITE_EMBEDDED_SOURCES "")
set(SQLITE_EMBEDDED_INCS "")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite3/sqlite3.c")
  set(EMBEDDED_SQLITE3 1)
  set(HAVE_SQLITE3 1)
  set(SQLITE_EMBEDDED_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite3/sqlite3.c")
  set(SQLITE_EMBEDDED_INCS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite3")
  set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite3/sqlite3.c"
    PROPERTIES COMPILE_DEFINITIONS
      "SQLITE_THREADSAFE=1;SQLITE_ENABLE_FTS5=1;SQLITE_ENABLE_JSON1=1;SQLITE_OMIT_LOAD_EXTENSION=1"
  )
else()
  find_package(SQLite3 QUIET)
  if(SQLite3_FOUND)
    set(HAVE_SQLITE3 1)
  endif()
endif()

if(EZSNMP_SOURCE_DIR STREQUAL "")
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ezsnmp/ezsnmp/include/sessionbase.h")
    set(EZSNMP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ezsnmp")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../ezsnmp/ezsnmp/include/sessionbase.h")
    set(EZSNMP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ezsnmp")
  endif()
endif()

set(HAVE_EZSNMP 0)
set(EZSNMP_EXTRA_LIBS "")

if(PkgConfig_FOUND AND NOT EZSNMP_SOURCE_DIR STREQUAL "")
  pkg_check_modules(NETSNMP netsnmp)
  if(NETSNMP_FOUND)
    set(HAVE_EZSNMP 1)
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND OR OPENSSL_FOUND)
      if(APPLE)
        list(APPEND EZSNMP_EXTRA_LIBS OpenSSL::Crypto OpenSSL::SSL)
      endif()
    endif()
    add_library(ezsnmp_session_core OBJECT
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
    if(OpenSSL_FOUND OR OPENSSL_FOUND)
      target_include_directories(ezsnmp_session_core PUBLIC ${OPENSSL_INCLUDE_DIR})
    endif()
    if(NOT MSVC)
      target_compile_definitions(ezsnmp_session_core PRIVATE __STRICT_ANSI__)
    endif()
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
  ${SQLITE_EMBEDDED_SOURCES}
  $<$<BOOL:${HAVE_EZSNMP}>:$<TARGET_OBJECTS:ezsnmp_session_core>>
)

target_include_directories(ezsnmp_data_logger PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  ${SQLITE_EMBEDDED_INCS}
)
if(HAVE_EZSNMP)
  target_include_directories(ezsnmp_data_logger PRIVATE
    "${EZSNMP_SOURCE_DIR}"
    "${EZSNMP_SOURCE_DIR}/ezsnmp/include"
    ${NETSNMP_INCLUDE_DIRS}
  )
  target_link_directories(ezsnmp_data_logger PUBLIC ${NETSNMP_LIBRARY_DIRS})
endif()

target_compile_definitions(ezsnmp_data_logger PUBLIC
  EZSNMP_DL_HAVE_EZSNMP=${HAVE_EZSNMP}
  EZSNMP_DL_HAVE_SQLITE3=${HAVE_SQLITE3}
  EZSNMP_DL_EMBEDDED_SQLITE3=${EMBEDDED_SQLITE3}
  EZSNMP_DL_HAVE_LIBPQ=$<BOOL:${PostgreSQL_FOUND}>
)

target_link_libraries(ezsnmp_data_logger PUBLIC Threads::Threads)
if(WIN32)
  target_link_libraries(ezsnmp_data_logger PUBLIC ws2_32)
endif()
if(HAVE_EZSNMP)
  target_link_libraries(ezsnmp_data_logger PUBLIC ${NETSNMP_LIBRARIES} ${EZSNMP_EXTRA_LIBS})
endif()
if(HAVE_SQLITE3 AND NOT EMBEDDED_SQLITE3)
  target_link_libraries(ezsnmp_data_logger PUBLIC SQLite::SQLite3)
endif()
if(PostgreSQL_FOUND)
  target_link_libraries(ezsnmp_data_logger PUBLIC PostgreSQL::PostgreSQL)
endif()

add_executable(ezsnmp-data-logger src/main.cpp)
target_link_libraries(ezsnmp-data-logger PRIVATE ezsnmp_data_logger)
