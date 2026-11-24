---
name: Auto Update Release Notes
concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: false
  job-discriminator: ${{ github.run_id }}
on:
  release:
    types: [published]
  workflow_dispatch:
    inputs:
      backfill_all:
        description: 'Backfill release notes for ALL existing releases'
        required: false
        default: 'false'
      additional_context:
        description: 'Optional extra context or instructions to incorporate when generating release notes'
        required: false
        default: ''
permissions:
  actions: read
  contents: read
  copilot-requests: write
timeout-minutes: 20
max-ai-credits: 40
model: sonnet
imports:
  - carlkidcrypto/shared-workflows/shared/auto-release-notes-core.md@eeefdd5c0a594d59ae86a09437b2de9bb57618a3
---

# Auto Update Release Notes

When a new release is published, generate and update its release notes body on GitHub using commit history, PR references, and grouped themes following the shared auto-release-notes core rules.

## Goals

- Produce high-signal, human-readable release notes for every tagged release.
- Group changes into themes (features, bug fixes, core engine, desktop packaging, multi-language bindings, frontends, CI/workflows, docs, dependencies, chores).
- Reference PR numbers and issue numbers where discoverable from commit messages.
- Include install instructions for Python (pip), npm, NuGet, WinGet, and standalone desktop binaries.
- Fully overwrite the release body with the generated notes — no content from the previous body is preserved.
- Skip any release whose body contains the exact marker `<!-- PROTECTED -->` — these are manually curated and must never be touched.
- When triggered manually with `backfill_all: true`, reformat ALL existing releases to the same standard.
