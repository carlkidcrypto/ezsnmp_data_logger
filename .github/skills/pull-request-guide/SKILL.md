---
name: pull-request-guide
description: Use when preparing, verifying, staging, opening, or replying to Pull Requests in ezsnmp_data_logger according to maintainer policies.
---

# Pull Request Guide (ezsnmp_data_logger)

Use this skill when preparing code changes, running verification checks, staging files, creating conventional commits, and opening or updating pull requests for `ezsnmp_data_logger`.

## 1. Strict Policies & Governance

> [!CRITICAL]
> **MAINTAINER-ONLY PR MERGING & PUBLISHING**:
> - Pull request merging is **exclusively reserved for the repository maintainer (@carlkidcrypto)**.
> - Never run `gh pr merge`, `git merge <branch> main`, `--admin`, or `--squash`.
> - Never publish GitHub releases or create git release tags.
> - Your responsibility ends with opening the PR, ensuring all CI checks pass, and summarizing the changes for maintainer review.

---

## 2. Review Comment & Mail Item Replies

Whenever review feedback or mail items on pull requests are addressed (whether from automated reviewers or human maintainers), **always** reply directly to every review comment thread on GitHub:
- Each reply must explicitly include:
  1. A concise description of the architectural or code fix implemented.
  2. The exact commit hash in which the fix was committed:
     ```
     Addressed in commit `<commit_sha>`: <description>
     ```
- Post threaded replies via GitHub CLI:
  ```bash
  gh api repos/{owner}/{repo}/pulls/{pull_number}/comments -F in_reply_to={comment_id} -f body="..."
  ```

---

## 3. Targeted Staging & Commit Conventions

- **Targeted Staging**: Always stage specific files explicitly:
  ```bash
  git add <file1> <file2>
  ```
  **NEVER** use blanket staging (`git add .`, `git add -A`).
- **Commit Messages**: Follow Conventional Commits:
  `feat(...)`, `fix(...)`, `chore(...)`, `docs(...)`, `test(...)`, `ci(...)`.

---

## 4. Pre-PR Verification Checklist

Before pushing commits or opening a PR:
1. Python code formatting: `black --check setup.py ezsnmp_data_logger tools python_tests`
2. C++ code formatting: `clang-format -n --Werror include/ezsnmp_data_logger/*.h src/*.cpp cpp_tests/*.cpp`
3. Software requirements: `awk 'length > 80 { print NR, length, $0 }' sphinx_docs_build/source/software_requirements.rst`
4. Unit tests: `pytest python_tests/`
5. Sphinx documentation: `sphinx-build -b html -W sphinx_docs_build/source sphinx_docs_build/build/html`
