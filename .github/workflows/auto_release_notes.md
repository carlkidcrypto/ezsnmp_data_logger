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
safe-outputs:
  noop:
    report-as-issue: true
  update-release:
timeout-minutes: 20
max-ai-credits: 40
model: sonnet
engine:
  id: copilot
network:
  allowed: [defaults, github, python]
tools:
  bash: true
---

# Auto Update Release Notes

When a new release is published, generate and update its release notes body on GitHub using commit history, PR references, and grouped themes.

## Goals

- Produce high-signal, human-readable release notes for every tagged release.
- Group changes into themes (features, bug fixes, core engine, desktop packaging, multi-language bindings, frontends, CI/workflows, docs, dependencies, chores).
- Reference PR numbers and issue numbers where discoverable from commit messages.
- Include install instructions for Python (pip), npm, NuGet, WinGet, and standalone desktop binaries.
- Fully overwrite the release body with the generated notes — no content from the previous body is preserved.
- Skip any release whose body contains the exact marker `<!-- PROTECTED -->` — these are manually curated and must never be touched.
- When triggered manually with `backfill_all: true`, reformat ALL existing releases to the same standard.

## Steps

0. **Token & AIC Optimization (Helper Script)**:
   Use the deterministic helper script `.github/scripts/generate_release_notes.py` to minimize token consumption and AI Credit (AIC) usage.

   - If triggered by `release: published`:
     Determine the published tag name from the release event, then execute:
     ```bash
     mkdir -p /tmp/gh-aw/agent
     python3 .github/scripts/generate_release_notes.py --tag <tag_name> --output-file /tmp/gh-aw/agent/release_notes.md
     ```
     (Pass `--additional-context "<context>"` if `additional_context` input is provided).
     Review `/tmp/gh-aw/agent/release_notes.md` and use the `update_release` tool (or run with `--publish`) to update the release.

   - If triggered by `workflow_dispatch` with `backfill_all: true`:
     Execute:
     ```bash
     python3 .github/scripts/generate_release_notes.py --backfill --publish
     ```

## Constraints

- Only update GitHub release bodies.
- Never edit releases containing `<!-- PROTECTED -->`.
- Never choose a prerelease base for a stable release if an earlier stable release exists.
