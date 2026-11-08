---
name: python-testing
description: Use when authoring, running, or debugging pytest test suites in python_tests/, measuring test coverage, or testing CLI and GUI frontends.
---

# Python Testing Guide (ezsnmp_data_logger)

Use this skill when developing, executing, or debugging Python unit tests and integration tests in `python_tests/` for `ezsnmp_data_logger`.

## 1. Environment & Prerequisites

- **Python Runtime**: Python 3.11, 3.12, 3.13, or 3.14.
- **Dependencies**: Install development and test dependencies:
  ```bash
  pip install -r requirements.txt
  pip install pytest pytest-cov pytest-sugar requests-mock black
  ```
- **Native Extension**: Build the SWIG Python extension in-place before running tests:
  ```bash
  python setup.py build_ext --inplace
  ```

---

## 2. Test Suite Structure

```
python_tests/
├── test_swig_bindings.py           # Core SWIG C++ class bindings and sinks
├── test_swig_multilang.py          # SWIG multi-language bindings verification
├── test_gui_bridge_and_frontends.py# CLI bridge, Desktop Tk, Web Studio HTTP server
├── test_desktop_packaging.py       # Standalone desktop packaging tools (.exe, .dmg, .deb, .rpm)
└── test_setup_parallel.py          # Parallel C++ compilation engine and caching
```

---

## 3. Running Tests

### A. Run Entire Test Suite
```bash
pytest python_tests/
```

### B. Run Specific Test Module or Function
```bash
# Run desktop GUI and server tests
pytest python_tests/test_gui_bridge_and_frontends.py

# Run a specific test function
pytest python_tests/test_gui_bridge_and_frontends.py -k "test_web_server_dashboard"
```

### C. Run with Coverage
```bash
pytest python_tests/ --cov=ezsnmp_data_logger --cov-report=term-missing
```

### D. Formatting & Linting
```bash
black --check setup.py ezsnmp_data_logger tools python_tests
```
