---
name: cpp-swig-development
description: Use when developing, refactoring, or bugfixing C++17 core telemetry engine code, SWIG interface definitions, or multi-language bindings (Python, Node.js, C#, Go).
---

# C++ & SWIG Development Guide (ezsnmp_data_logger)

Use this skill when developing, refactoring, or fixing bugs in the C++ layer, SWIG interface files, or 4-language bindings for `ezsnmp_data_logger`.

## 1. Architecture & Multi-Language Scope

The core SNMP telemetry engine is implemented in modern C++17 and wrapped into **4 official languages** via SWIG:
1. **Python** (C-Extension via setuptools & cibuildwheel)
2. **JavaScript / Node.js** (N-API native addon via cmake-js & npm)
3. **C#** (.NET 8+ P/Invoke class library via dotnet & NuGet)
4. **Go** (cgo package)
*(Java is explicitly excluded per ecosystem standards).*

```
ezsnmp_data_logger/
├── include/ezsnmp_data_logger/  # Public C++17 headers
├── src/                         # Native C++17 implementations
├── interface/                   # SWIG interface definition files
│   └── ezsnmp_data_logger.i
├── bindings/
│   ├── javascript/              # Node.js N-API bindings
│   ├── csharp/                  # C# .NET 8 bindings
│   └── go/                      # Go cgo bindings
└── tools/
    └── generate_swig_bindings.py# Automated multi-language SWIG generator
```

> [!CRITICAL]
> **Never modify SWIG-generated files directly**:
> - Forbidden: `src/ezsnmp_data_logger_wrap.cxx`
> - Forbidden: `ezsnmp_data_logger/ezsnmp_data_logger.py`
> - Forbidden: `bindings/*/generated/*`
> Always edit source headers in `include/`, source files in `src/`, or interface definitions in `interface/ezsnmp_data_logger.i`, and re-run generation.

---

## 2. Generating SWIG Wrappers

Generate wrappers across all 4 languages:
```bash
python tools/generate_swig_bindings.py --all
```

Or for a specific language:
```bash
python tools/generate_swig_bindings.py --languages python javascript csharp go
```

---

## 3. C++17 Standards & Net-SNMP Conformance

1. **Standard**: All C++ code must strictly conform to **C++17** (`-std=c++17`).
2. **Net-SNMP Macro Compatibility**: Always ensure `set(CMAKE_CXX_EXTENSIONS OFF)` and define `__STRICT_ANSI__` on non-MSVC builds to avoid GCC C-only `__builtin_types_compatible_p` macro expansion issues in Net-SNMP headers.
3. **Formatting**: Format all C++ code using clang-format matching `.clang-format`:
   ```bash
   clang-format -i include/ezsnmp_data_logger/*.h src/*.cpp cpp_tests/*.cpp
   ```
