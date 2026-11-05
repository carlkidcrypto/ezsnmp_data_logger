cmake_minimum_required(VERSION 3.16)
project(ezsnmp_data_logger_csharp VERSION 2026.11.5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(../.. ezsnmp_data_logger_core)

add_library(ezsnmp_data_logger_pinvoke SHARED
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/csharp/ezsnmp_data_logger_wrap.cxx"
)
set_target_properties(ezsnmp_data_logger_pinvoke PROPERTIES OUTPUT_NAME "ezsnmp_data_logger")
target_include_directories(ezsnmp_data_logger_pinvoke PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../include"
)
target_link_libraries(ezsnmp_data_logger_pinvoke PRIVATE ezsnmp_data_logger)
