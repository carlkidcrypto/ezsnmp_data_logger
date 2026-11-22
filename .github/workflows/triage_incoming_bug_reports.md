---
name: Triage Incoming Bug Reports
on:
  roles: all
  workflow_dispatch:
  issues:
    types: [opened, edited, reopened]
permissions:
  actions: read
  contents: read
  issues: read
  pull-requests: read
  copilot-requests: write
timeout-minutes: 10
max-ai-credits: 15
model: sonnet
imports:
  - carlkidcrypto/shared-workflows/shared/triage-bug-reports-core.md@22f04e222fa07048891327e4d34bb174d36e7bfa
  - carlkidcrypto/shared-workflows/shared/supplemental-labels.md@22f04e222fa07048891327e4d34bb174d36e7bfa
---

# Bug Report Intake Triage

Triage incoming bug reports in `carlkidcrypto/ezsnmp_data_logger` following the shared bug report intake triage core rules.

## Required Template Sections (Score 0–8)

Validate these 8 required template sections are present and meaningfully filled:

1. EzSnmpDataLogger release version OR commit number
2. Operating System and Version
3. Execution Interface (CLI / Desktop GUI / Web GUI / Python / C# / Node.js / Go)
4. Runtime Version (e.g. Python 3.11+, Node.js 24, .NET 8, Go 1.23, or Standalone Binary)
5. Describe the bug
6. To Reproduce (concrete reproducible steps)
7. Expected behavior
8. Additional context (logs, stack trace, config file, database/sink details, or explicit statement that none is available)

## Triage Comment Details

- **Incomplete Report (score < 8)**: Include the intake score (`x/8`), a checkbox list of missing or weak sections, a copy-paste reporter checklist asking for exact missing details, and a friendly note that triage will continue automatically after they edit the issue.
- **Complete Report (score = 8)**: Include the intake score (`8/8`), a short summary of environment and reproduction signal quality, and a note that the issue is ready for technical investigation.
