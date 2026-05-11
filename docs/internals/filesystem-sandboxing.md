# Filesystem Sandboxing in Neolith

**Status**: Implemented (hardened path anchoring for include/compiler/object/binary flows and file access efuns)
**Audience**: Driver developers maintaining compiler, loader, and runtime file I/O
**Created**: 2026-05-08

---

## Executive Summary

Neolith treats the mudlib directory as a filesystem sandbox root. Historically (MudOS style), many file operations were implicitly sandboxed by:

1. changing process working directory (`chdir`) to mudlib at startup, and
2. stripping leading `/` from LPC paths so they became relative paths.

This approach is fragile because process CWD is global mutable state. Any code (driver, dependency, or platform helper) that changes CWD can silently redirect file access.

Neolith hardens this by computing and storing a verified absolute mudlib root at startup, then using that root explicitly in critical path resolution logic.

For file access efuns, Neolith also moved path validation/anchoring to a push-style API that is re-entrant safe and unwind-safe.

---

## Threat Model and Failure Mode

### MudOS-era fragility

The classic pattern assumes:

- CWD always remains mudlib.
- Relative paths are therefore safe and implicitly rooted.

When that assumption breaks, sandbox boundaries can weaken:

- source/object loads can resolve against the wrong directory,
- include searches can resolve outside intended tree,
- binary freshness checks can read wrong files.

### Security objective

For driver-internal file I/O in compile/load paths:

- resolve through an explicit, verified mudlib root,
- reject path traversal attempts,
- avoid CWD dependence for correctness/security.

---

## Neolith Hardening Architecture

### 1) Verified mudlib root captured at startup

Startup still validates mudlib by changing CWD, then persists the canonical absolute path:

- `CHDIR(CONFIG_STR(__MUD_LIB_DIR__))` in [src/main.c](../../src/main.c)
- `realpath(".", MAIN_OPTION(mudlib_dir_absolute))` in [src/main.c](../../src/main.c)
- field definition in [src/main.h](../../src/main.h)

This converts a runtime assumption ("CWD is mudlib") into explicit state (`mudlib_dir_absolute`) reusable by subsystems.

### 2) Include resolution (`inc_open`) anchored to verified root

`inc_open` now reads mudlib root from `MAIN_OPTION(mudlib_dir_absolute)` instead of raw config string and uses explicit path joins.

Key locations:

- include open path logic in [lib/lpc/lex.c](../../lib/lpc/lex.c)
- sandbox containment check `is_path_within_root(...)` in [lib/lpc/lex.c](../../lib/lpc/lex.c)
- include path normalization in `set_inc_list(...)` in [lib/lpc/lex.c](../../lib/lpc/lex.c)

`set_inc_list` normalization aligns with `inc_open` expectations:

- leading `/` means "from mudlib root" for include search entries,
- trailing `/` removed,
- empty entry means mudlib root search.

### 3) Object source load path (`load_object`) anchored to verified root

`load_object` keeps LPC object naming semantics (`name`, `real_name`) but builds `source_path` for host filesystem operations using the verified mudlib root.

Key locations:

- `source_path` construction in [src/simulate.c](../../src/simulate.c)
- `stat(source_path)` in [src/simulate.c](../../src/simulate.c)
- `FILE_OPEN(source_path, ...)` in [src/simulate.c](../../src/simulate.c)

A compatibility fallback remains for non-main contexts where `mudlib_dir_absolute` may be unset (for example, some isolated test setups).

### 4) Binary save/load and freshness checks anchored to verified root

Binary path and source-time checks are no longer implicitly CWD-relative in the hardened path:

- `make_binary_path(...)` uses verified mudlib root when available in [lib/lpc/program/binaries.cpp](../../lib/lpc/program/binaries.cpp)
- `make_source_path(...)` + `check_times(...)` resolve through verified mudlib root in [lib/lpc/program/binaries.cpp](../../lib/lpc/program/binaries.cpp)

### 5) Opcode config-id freshness check hardened

`compute_opcode_config_id()` previously depended on relative stat behavior for simul_efun timestamp checks.

It now resolves `__SIMUL_EFUN_FILE__` against verified mudlib root when available.

- see [lib/lpc/compiler.c](../../lib/lpc/compiler.c)

### 6) File access efuns hardened with stack-owned path contracts

Historically, path validation helpers for file efuns relied on retained/static path storage (`check_valid_path`), which is fragile when master applies re-enter file efuns (for example, `valid_read` / `valid_write` side effects).

Neolith now hardens file efun path handling with two helpers in [lib/efuns/file_utils.c](../../lib/efuns/file_utils.c) declared in [lib/efuns/file_utils.h](../../lib/efuns/file_utils.h):

- `push_valid_path(path, caller, call_fun, writeflg)` (replaces `check_valid_path`)
  - runs master object permission applies,
  - normalizes to legal relative path,
  - pushes validated path on eval stack.
- `push_resolved_valid_path(path, caller, call_fun, writeflg)`
  - performs `push_valid_path` stage,
  - resolves to sandboxed absolute path using `resolve_path_in_mudlib`,
  - pushes resolved path on eval stack.

Key hardening properties:

- no shared static return buffer; each call owns its stack slot,
- nested/re-entrant permission apply flows cannot overwrite sibling path results,
- on LPC `error()` unwind, stack-owned temporary paths are automatically freed,
- no fallback to implicit CWD-based resolution when `mudlib_dir_absolute` is unset.

Path legality and root containment are centralized in C++17 filesystem helpers:

- [lib/misc/filepath.cpp](../../lib/misc/filepath.cpp)
- [lib/misc/filepath.h](../../lib/misc/filepath.h)

This hardening is now used by core file efun paths (read/write/remove/stat/rename/cp), dump helpers, editor file access, and save/restore object path flows.

---

## MudOS vs Neolith: Behavioral Contrast

### MudOS-style implicit sandboxing

- Global CWD is security boundary.
- Leading slash stripping converts paths to relative.
- Subsystems rely on implicit process state.

### Neolith hardened model

- Verified absolute mudlib root stored once.
- Critical subsystems perform explicit root-based path resolution.
- Include path traversal blocked with containment checks.
- File access efuns use stack-owned validated/resolved paths instead of retained static path storage.
- CWD changes are less likely to break sandbox assumptions.

---

## Test Coverage Added

Regression tests validate behavior outside mudlib CWD:

- include path via compiler/load flow:
  - [tests/test_lpc_compiler/test_lpc_compiler.cpp](../../tests/test_lpc_compiler/test_lpc_compiler.cpp)
- object load path anchoring:
  - [tests/test_lpc_compiler/test_lpc_compiler.cpp](../../tests/test_lpc_compiler/test_lpc_compiler.cpp)
- binary load path anchoring:
  - [tests/test_lpc_compiler/test_save_binary.cpp](../../tests/test_lpc_compiler/test_save_binary.cpp)

These tests ensure path-sensitive operations remain correct when current working directory is intentionally moved away from mudlib.

File-efun hardening is exercised through existing efun/object test coverage (including `cp`/`save_object` paths) and full unit-test runs after migration.

---

## Remaining Considerations

1. CWD is still process-global state, and some legacy/auxiliary code may still assume mudlib CWD.
2. Hardening now covers compiler/include/object/binary flows and core file access efun call paths.
3. New file I/O entry points should prefer explicit root-based joins and push-style stack ownership over retained/static path storage.

---

## Developer Guidance

When adding filesystem access in driver internals:

1. Prefer `MAIN_OPTION(mudlib_dir_absolute)` as root anchor.
2. Normalize untrusted path fragments before joining.
3. Enforce containment checks where traversal can appear.
4. For efun-facing file paths, use `push_valid_path` / `push_resolved_valid_path` and pop stack slots in all non-error paths.
5. Add tests that run with CWD outside mudlib to catch regressions.
