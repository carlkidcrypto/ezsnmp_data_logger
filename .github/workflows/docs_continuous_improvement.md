---
name: Docs Continuous Improvement
on:
  workflow_dispatch:
  schedule: every 3 days
  skip-if-match:
    query: 'is:pr is:open head:automation/docs-continuous-improvement label:documentation label:automated-pr'
permissions:
  copilot-requests: write
  actions: read
  contents: read
timeout-minutes: 15
max-ai-credits: 25
model: sonnet
imports:
  - carlkidcrypto/shared-workflows/shared/docs-continuous-improvement-core.md@3538efdfabdbcf0ae956cfc5685fd4c21a59696b
  - carlkidcrypto/shared-workflows/shared/pr-preflight-checklist.md@3538efdfabdbcf0ae956cfc5685fd4c21a59696b
---

# Documentation Continuous Improvement

Review and improve repository documentation gradually over time following the shared documentation continuous improvement core rules.

## Scope

Audit and improve:

- `README.rst`
- `docs/**`
- `sphinx_docs_build/source/**`
- `HOWTOAI.rst`
- inline docstrings in Python files under `ezsnmp_data_logger/**`
- C++ doc comments in `include/ezsnmp_data_logger/**` and `src/**`
- Software Requirements Specification in `sphinx_docs_build/source/software_requirements.rst`

## Repository-Specific Requirements

- **Software Requirements Specification Integrity**: If editing `sphinx_docs_build/source/software_requirements.rst`, strictly enforce a maximum line length of 80 characters per line and ensure each requirement block is separated by a blank line. Each requirement shall test exactly one feature or function.
- Avoid changing SWIG generated wrappers (`src/ezsnmp_data_logger_wrap.cxx`).

## Goals

- Fix typos, grammar, and broken wording.
- Fix inaccurate or misleading statements regarding SNMP polling, storage sinks, CLI flags, GUI operations, or multi-language bindings.
- Improve clarity where current text could cause user confusion.
- Add or correct missing/incorrect Python docstrings or C++ comments for public functions/classes.
- Keep edits small and focused each run (no massive rewrites).
- When opening the PR, consult the `pull-request-guide` skill.

## skill: `pull-request-guide`
---
description: Conventions and required body contents for documentation improvement PRs.
---

### Pull Request Output

When changes are made, create or update one PR:
- Branch: `automation/docs-continuous-improvement`
- Base: `main`
- Title style: `[docs-improvement] <short summary>`

PR body must include:
- Files updated
- Types of improvements (typos, clarifications, docstrings, etc.)
- Any follow-up documentation gaps discovered
