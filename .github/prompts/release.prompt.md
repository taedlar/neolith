---
agent: agent
description: >
  Automate the Neolith release process: validate state, gather commits, update
  ChangeLog.md, bump CMakeLists.txt version, tag the release, and create a
  GitHub release with generated notes.
tools: [vscode, execute, read, agent, edit, search, web, browser, 'github/*', ms-vscode.cpp-devtools/Build_CMakeTools, ms-vscode.cpp-devtools/RunCtest_CMakeTools, ms-vscode.cpp-devtools/ListBuildTargets_CMakeTools, ms-vscode.cpp-devtools/ListTests_CMakeTools, todo]
---

You are performing a Neolith driver release. Follow every step in order. Stop
and ask the user before any destructive or irreversible action (git push,
tagging, GitHub release creation).

## Context

- Repository: `h:\github\neolith`
- Version source: `CMakeLists.txt`, line starting with `VERSION` inside the
  `project()` block. The format is `MAJOR.MINOR.PATCH`.
- Release notes: `docs/ChangeLog.md`. Format for a new section:
  ```
  ### X.Y.Z-<qualifier> — YYYY-MM-DD

  #### Changes since X.Y.Z-<prev>
  - <sha7> <type>: <description> (#<PR>)
  ```
- Binary compatibility sentinel: `lib/lpc/program/binaries.c`, `driver_id`
  (hex date stamp). Bump only when opcodes or runtime struct sizes changed.

## Step 1 — Verify working tree is clean

Run:
```
git -C h:\github\neolith status --short
```

If there are uncommitted changes, stop and tell the user. Do not proceed until
the working tree is clean.

## Step 2 — Determine current and new version

1. Read `CMakeLists.txt` and extract the current `VERSION` value.
2. Read `docs/ChangeLog.md` and identify the most recently released version
   (the first `###` heading that has a date, not "unreleased").
3. Ask the user: **"What should the new version be?"** — show the current
   version and suggest a reasonable increment (patch bump by default).
   Also ask: **"Is this a pre-release? If so, what qualifier? (e.g. alpha.8)"**

## Step 3 — Collect commits since last release

Run:
```
git -C h:\github\neolith log <last-tag>..HEAD --oneline --no-merges
```

Where `<last-tag>` is the git tag for the previously released version (e.g.
`v1.0.0-alpha.7`). If the tag doesn't exist, use the SHA from the ChangeLog.

Group the commits by conventional-commit prefix:
- `feat:` → Features
- `fix:` → Bug Fixes
- `clean:` / `refactor:` → Cleanup
- `chore:` / `ci:` → Chores
- `doc:` → Documentation
- everything else → Other

## Step 4 — Update docs/ChangeLog.md

Insert a new section immediately after the `## neolith-X.Y.Z (unreleased)`
heading (or replace that heading if this is the final release of that
milestone). Use today's date (ISO 8601). Example:

```markdown
### 1.0.0-alpha.8 — 2026-03-25

#### Changes since 1.0.0-alpha.7
- abc1234 feat: add cool feature (#300)
- def5678 fix: crash in interpreter (#299)
```

Show the user the proposed diff before writing.

## Step 5 — Bump version in CMakeLists.txt

Replace the `VERSION` line in `CMakeLists.txt` with the new version
(MAJOR.MINOR.PATCH only — no qualifier; qualifiers go in ChangeLog and git
tags).

## Step 6 — Check driver_id

Ask the user: **"Were any opcodes added, removed, or reordered, or were any
runtime structs resized since the last release?"**

If yes: open `lib/lpc/program/binaries.c` and update `driver_id` to a new hex
date stamp reflecting today (format `0xYYYYMMDD`). Show them the proposed
change.

## Step 7 — Run tests

Run the CI preset tests and report results:
```
cmake --build --preset ci-linux
ctest --preset ut-linux
```
(On Windows without WSL, use `ci-vs16-x64` / `ut-vs16-x64` or
`ci-clang-x64` / `ut-clang-x64`.)

Do not proceed if tests fail. Summarize any failures for the user.

## Step 8 — Commit release changes

Show the user the list of files changed (ChangeLog.md, CMakeLists.txt, and
optionally binaries.c). Ask for confirmation, then run:

```
git -C h:\github\neolith add docs/ChangeLog.md CMakeLists.txt lib/lpc/program/binaries.c
git -C h:\github\neolith commit -m "chore: release v<NEW_VERSION>"
```

## Step 9 — Create and push the tag

Ask the user to confirm before running:

```
git -C h:\github\neolith tag -a v<NEW_VERSION> -m "Release v<NEW_VERSION>"
git -C h:\github\neolith push origin main --follow-tags
```

## Step 10 — Create GitHub release

Using the GitHub MCP tool, create a GitHub release for the tag `v<NEW_VERSION>`
with:
- **Title**: `neolith v<NEW_VERSION>`
- **Body**: the ChangeLog section you wrote in Step 4 (markdown)
- **Pre-release**: true if a qualifier (alpha/beta/rc) is present, false otherwise

Ask the user to confirm before creating.

## Done

Summarize what was done: version bumped, ChangeLog updated, tag pushed, release
created. Mention any steps that were skipped and why.
