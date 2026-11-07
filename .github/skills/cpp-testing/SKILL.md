---
name: cpp-testing
description: Use when building, executing, or debugging C++17 GoogleTest suites in cpp_tests/ with Meson, Ninja, or CMake.
---

# C++ Testing Guide (ezsnmp_data_logger)

Use this skill when developing, building, or running native C++17 GoogleTest test suites in `cpp_tests/` for `ezsnmp_data_logger`.

## 1. Test Suite Architecture

```
cpp_tests/
├── meson.build                  # Meson test configuration
├── CMakeLists.txt               # CMake test configuration
├── test_config_and_datatypes.cpp# Configuration parsing and datatype mapping
├── test_data_logger.cpp         # Data logging engine, storage sinks, polling
└── test_sink_inspector.cpp      # Runtime sink metrics inspection
```

---

## 2. Building & Running with Meson & Ninja

Meson is the preferred fast build tool for native testing:

```bash
# Setup build directory
meson setup build-cpp-tests -Dbuild_tests=true

# Compile test binaries
ninja -C build-cpp-tests

# Run all GoogleTests
ninja -C build-cpp-tests test

# Or run test executable directly with gtest flags
./build-cpp-tests/cpp_tests/ezsnmp_data_logger_tests --gtest_filter="*Config*"
```

---

## 3. Building & Running with CMake

```bash
cmake -B build-cmake -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-cmake -j
ctest --test-dir build-cmake --output-on-failure
```

---

## 4. Code Formatting Verification

Ensure all C++ test files conform to `.clang-format`:
```bash
clang-format -n --Werror cpp_tests/*.cpp include/ezsnmp_data_logger/*.h src/*.cpp
```
