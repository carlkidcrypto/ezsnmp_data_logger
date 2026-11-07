---
name: Coverage Autofix Python
on:
  schedule:
    - cron: "0 9 */3 * *"
  workflow_dispatch:
  skip-if-match:
    query: 'is:pr is:open head:automation/coverage-autofix-python label:automated-pr'
permissions:
  actions: read
  contents: read
  copilot-requests: write
safe-outputs:
  noop:
    report-as-issue: true
  create-pull-request:
    title-prefix: "[coverage-autofix-py] "
    labels: [automated-pr, coverage, tests, python]
    draft: true
    preserve-branch-name: true
    if-no-changes: "ignore"
    allowed-files:
      - "python_tests/**"
      - "ezsnmp_data_logger/**"
      - ".github/scripts/**"
    excluded-files:
      - ".github/workflows/**"
timeout-minutes: 20
max-ai-credits: 40
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

# Python Coverage Checks And Suggested Fixes

Run an end-to-end coverage health check for Python tests, then propose and
implement minimal, safe fixes that improve coverage and reliability.

## Hard Requirements

- Focus only on this repository (`ezsnmp_data_logger`).
- Keep changes scoped and low-risk. Limit each run to at most 1 target test file (1–3 focused test cases).
- Prefer tests first when improving coverage.
- Do not open a new pull request if an open automation PR already exists for
  branch `automation/coverage-autofix-python`.
- If no meaningful change is needed, make no file edits and end cleanly.
- **Bounded file reads (Token Optimization)**:
  Files larger than 20 KB must **not** be read in full. Use targeted `grep`, `head`,
  `tail`, or line-range views. Avoid dumping full source or test files into context.

## Coverage Check Procedure

1. Use Codecov to identify coverage gaps:
   - Start with pre-fetched Codecov data in `/tmp/gh-aw/agent/codecov/summary.md`.
   - Inspect files under `ezsnmp_data_logger/` and existing tests under `python_tests/`.
   - Author targeted test cases in `python_tests/` (e.g. `test_swig_bindings.py`, `test_gui_bridge_and_frontends.py`, `test_desktop_packaging.py`).

2. Determine if action is needed:
   - If coverage indicates clear gaps or uncovered branches, create focused tests.
   - If coverage is healthy and no concrete improvement is justified, call `noop`.

## Fix Strategy

- Prioritize:
  - Adding missing test coverage for uncovered branches in CLI runner, GUI launcher, server, and packaging helpers.
  - Fixing brittle tests or edge case handling.
- Never use blanket staging commands (`git add .`, `git add -A`). Explicitly stage target files.
