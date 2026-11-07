---
name: Coverage Autofix C++
on:
  schedule:
    - cron: "0 9 */3 * *"
  workflow_dispatch:
  skip-if-match:
    query: 'is:pr is:open head:automation/coverage-autofix-cpp label:automated-pr'
permissions:
  actions: read
  contents: read
  copilot-requests: write
safe-outputs:
  noop:
    report-as-issue: true
  create-pull-request:
    title-prefix: "[coverage-autofix-cpp] "
    labels: [automated-pr, coverage, tests, cpp]
    draft: true
    preserve-branch-name: true
    if-no-changes: "ignore"
    allowed-files:
      - "cpp_tests/**"
      - "src/**"
      - "include/ezsnmp_data_logger/**"
      - ".github/scripts/**"
    excluded-files:
      - ".github/workflows/**"
      - "src/ezsnmp_data_logger_wrap.cxx"
timeout-minutes: 20
max-ai-credits: 60
model: sonnet
engine:
  id: copilot
network:
  allowed: [defaults, python, api.github.com]
tools:
  edit:
  bash: true
imports:
  - shared/codecov-coverage.md
---

# C++ Coverage Checks And Suggested Fixes

Run an end-to-end coverage health check for C++ GoogleTests, then
propose and implement minimal, safe fixes that improve coverage and reliability.

## Hard Requirements

- Focus only on this repository (`ezsnmp_data_logger`).
- Keep changes scoped and low-risk. Limit each run to at most 1 target test file (1–3 focused test cases).
- Prefer tests first when improving coverage.
- Allowed files: `cpp_tests/**`, `src/**`, `include/ezsnmp_data_logger/**`, and `.github/scripts/**`.
- Forbidden: NEVER touch SWIG generated files (`src/ezsnmp_data_logger_wrap.cxx`) or workflow files.
- Staging: Use targeted staging only (`git add <file>`), never `git add .` or `git add -A`.

## Coverage Check Procedure

1. Inspect Codecov summary in `/tmp/gh-aw/agent/codecov/summary.md`.
2. Map uncovered lines in `src/` (e.g. `config.cpp`, `data_logger.cpp`, `json_utils.cpp`, `sink_inspector.cpp`, sinks) to test cases in `cpp_tests/`.
3. Author GoogleTest cases statically adhering to modern C++17.
4. Format changes using `clang-format`.
