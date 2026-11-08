---
name: Python Version Watcher

on:
  schedule:
    - cron: "0 9 1 * *"
  workflow_dispatch:
  skip-if-match:
    query: "is:pr is:open head:automation/python-version-watcher label:automated-pr"

permissions:
  actions: read
  contents: read
  copilot-requests: write

safe-outputs:
  noop:
    report-as-issue: true
  create-pull-request:
    title-prefix: "[python-version]"
    labels:
      - automated-pr
    draft: true
    preserve-branch-name: true
    if-no-changes: ignore
    protected-files: allowed

timeout-minutes: 15
max-ai-credits: 25

model: sonnet
engine:
  id: copilot
  args: ["--allow-all-tools"]
network:
  allowed:
    - defaults
    - devguide.python.org

tools:
  edit:
  bash: true
---

## Python Version Watcher and Auto-Sync

Monitor https://devguide.python.org/versions/ for changes to the set of actively supported Python
versions and automatically update the repository to stay in sync.

## Goals

- Detect which Python versions are currently supported (status = "bugfix", "security", or
  "prerelease") vs. end-of-life (status = "end-of-life").
- Update `pyproject.toml` (`requires-python`, `[tool.black]` `target-version` list, and
  `Programming Language :: Python :: 3.x` classifier lines) to reflect the current supported set.
- Update `.github/workflows/tests_python.yml` (`python-version: [...]` matrix list) to reflect
  the current supported set.
- Open a draft PR only when changes are actually needed.

## Steps

### 1. Fetch the Python versions page

Use `curl` to download the Python versions page:

```bash
curl -fsSL https://devguide.python.org/versions/ -o /tmp/gh-aw/agent/pyversions.html
```

- If the command fails (non-zero exit code), stop immediately and report the error.
- Check that `/tmp/gh-aw/agent/pyversions.html` is at least 500 bytes. If not, stop and report that the
  page content looks truncated or empty.

### 2. Parse supported versions

Use Python to extract the currently supported version numbers from the downloaded page:

```python
import re

with open('/tmp/gh-aw/agent/pyversions.html') as f:
    content = f.read()

rows = re.findall(
    r'<tr[^>]*>.*?</tr>',
    content,
    re.DOTALL | re.IGNORECASE
)

supported = []
for row in rows:
    ver_match = re.search(r'\b(3\.\d+)\b', row)
    if not ver_match:
        continue
    version = ver_match.group(1)
    if re.search(r'\b(bugfix|security|prerelease)\b', row, re.IGNORECASE):
        supported.append(version)

supported = sorted(set(supported), key=lambda v: int(v.split('.')[1]))
print(supported)
```

- If fewer than 3 supported versions are found, stop and report a parsing error (the page
  structure may have changed).
- Log the list of discovered supported versions before proceeding.

### 3. Read current repo state (Bounded File Reads — Token Optimization)

Do **not** read workflow files in full. Use `grep -n "python-version"` or targeted line views:

- `pyproject.toml`: locate and read `requires-python`, `target-version` under `[tool.black]`, and all
  `Programming Language :: Python :: 3.x` classifier lines.
- `.github/workflows/tests_python.yml`: locate and read the `python-version: [...]` matrix line.

Extract the currently declared version list from these files and compare it to the
`supported` list obtained in Step 2.

### 4. Check whether updates are needed

Compute the expected new values:

- **`python-version` matrix list** in `.github/workflows/tests_python.yml`:
  A JSON-style array of the supported versions, e.g. `["3.12", "3.13"]`.
- **`requires-python`** in `pyproject.toml`:
  `>={min}` where `min` is the lowest supported version (e.g. `>=3.11`).
- **`target-version`** in `pyproject.toml` (under `[tool.black]`):
  A list of `pyXYZ` tokens, e.g. `["py311", "py312", "py313"]`.
- **Classifier lines** in `pyproject.toml`:
  One line per supported version:
  `    "Programming Language :: Python :: 3.x",`

If all current values already match the expected new values exactly, call the `noop` safe
output tool with a message like:
"No update needed. Supported versions are already [list]. All files are up to date."
Then stop.

### 5. Update `pyproject.toml`

Apply edits to `pyproject.toml`:
1. `requires-python`
2. `target-version` under `[tool.black]`
3. `Programming Language :: Python :: 3.x` classifiers

### 6. Update `.github/workflows/tests_python.yml`

Update the `python-version: [...]` matrix list in `.github/workflows/tests_python.yml`.

### 7. Open a draft PR

After edits are applied, create a pull request using:
- **Branch**: `automation/python-version-watcher`
- **Base**: `main`
- **Title**: `chore: sync supported Python versions`
- **Commit message**: `chore: sync supported Python versions to devguide.python.org`

Consult the `pull-request-guide` skill for the PR body details.

## skill: `pull-request-guide`
---
description: PR body format and details for python version watcher updates.
---

### Pull Request Body Requirements

Include:
- Source checked: https://devguide.python.org/versions/
- Newly detected supported versions
- Summary of changes made per file
- Note that this was generated automatically by the Python Version Watcher workflow
