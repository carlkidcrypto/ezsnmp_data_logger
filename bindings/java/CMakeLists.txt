cmake_minimum_required(VERSION 3.16)
project(ezsnmp_data_logger_jni LANGUAGES CXX Java)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(JNI REQUIRED)
find_package(Java REQUIRED)
include(UseJava)

add_subdirectory(../.. ezsnmp_data_logger_core)

add_library(ezsnmp_data_logger_jni SHARED
  "${CMAKE_CURRENT_BINARY_DIR}/generated/java/ezsnmp_data_logger_wrap.cxx"
)
target_include_directories(ezsnmp_data_logger_jni PRIVATE
  ${JNI_INCLUDE_DIRS}
  "${CMAKE_CURRENT_SOURCE_DIR}/../../include"
)
target_link_libraries(ezsnmp_data_logger_jni PRIVATE ezsnmp_data_logger)
