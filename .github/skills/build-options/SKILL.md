---
name: build-options
description: "Manage Neolith build-time options: add, change, or remove CMake options that control driver behavior and LPC visibility. Use when creating a new option, toggling an existing option default, renaming macros, tracing how an option surfaces to LPC code, or troubleshooting preprocessor define mismatches between cmake/options.cmake, lib/lpc/options.h.in, and the generated options.h."
---

# Build Options Skill

Neolith build-time options live in two connected files:

- [cmake/options.cmake](../../cmake/options.cmake) — CMake-level declarations (defaults visible to `cmake-gui`, `ccmake`, `-D` overrides)
- [lib/lpc/options.h.in](../../lib/lpc/options.h.in) — C/LPC-level template processed by `configure_file()` into `options.h`

Every option flows: **cmake/options.cmake → configure → options.h → C compilation + LPC predefines**.

## Build Pipeline

```
cmake/options.cmake          (option() / set() declarations)
        ↓  cmake configure
lib/lpc/options.h.in         (#cmakedefine directives)
        ↓  configure_file()
${CMAKE_BINARY_DIR}/options.h  (generated; included by config.h)
        ↓  C preprocessor
        ↓  #include "config.h"  (all source files)
        └─ edit_source POST_BUILD
                ↓  scans options.h for #define macros
efuns_option.h  → LPC predefined macro table  (__MACRO_NAME__)
```

Key CMake commands in [lib/lpc/CMakeLists.txt](../../lib/lpc/CMakeLists.txt):

```cmake
configure_file(options.h.in ${CMAKE_BINARY_DIR}/options.h @ONLY)
```

And the top-level [CMakeLists.txt](../../CMakeLists.txt):

```cmake
configure_file(config.h.in config.h @ONLY)  # includes options.h
```

`edit_source` runs POST_BUILD and scans `options.h` for `#define` macros, emitting each defined macro as `__MACRO_NAME__` into `efuns_option.h`, making it available to LPC code via the preprocessor's predefined table.

## Option Types

### Boolean (on/off feature flag)

**cmake/options.cmake:**
```cmake
option(MY_FEATURE "Enable my feature" ON)  # ON = default enabled
```

**lib/lpc/options.h.in:**
```c
#cmakedefine MY_FEATURE
```

Generated `options.h` when ON: `#define MY_FEATURE`
Generated `options.h` when OFF: `/* #undef MY_FEATURE */`

### String value

**cmake/options.cmake:**
```cmake
set(MY_PARAM "default_value" CACHE STRING "Description of parameter")
```

**lib/lpc/options.h.in:**
```c
#cmakedefine MY_PARAM "@MY_PARAM@"     /* for string literals */
#cmakedefine MY_PARAM @MY_PARAM@       /* for numeric/expression values */
```

Generated `options.h`: `#define MY_PARAM "default_value"` or `#define MY_PARAM 42`

### Dependent boolean

Use `cmake_dependent_option()` when one option only makes sense if another is enabled:

```cmake
cmake_dependent_option(STRIP_BEFORE_PROCESS_INPUT "..." ON "NO_ANSI" OFF)
```

This sets `STRIP_BEFORE_PROCESS_INPUT=ON` only when `NO_ANSI=ON`; otherwise forces it `OFF`.

## Adding a New Option

### Step 1 — Declare in cmake/options.cmake

Add to the appropriate section (Compatibility, Miscellaneous, or Performance tuning):

```cmake
option(MY_NEW_OPTION "What it does and its consequences" OFF)
```

or for a value:

```cmake
set(MY_NEW_VALUE 42 CACHE STRING "Description (units, range)")
```

### Step 2 — Add to lib/lpc/options.h.in

Add a `#cmakedefine` line in the matching section (keep sections in sync):

```c
/* MY_NEW_OPTION: brief description.
 */
#cmakedefine MY_NEW_OPTION
```

For values:
```c
/* MY_NEW_VALUE: brief description.
 */
#cmakedefine MY_NEW_VALUE @MY_NEW_VALUE@
```

### Step 3 — Use in C source

Guard C code normally:

```c
#ifdef MY_NEW_OPTION
  /* enabled path */
#endif
```

For value macros, they are always defined (no guard needed if the cmake default is always set):

```c
int limit = MY_NEW_VALUE;
```

### Step 4 — (Optional) LPC visibility

If LPC code needs to test the option, `edit_source` automatically exposes `__MY_NEW_OPTION__` as a predefined macro. In LPC:

```lpc
#ifdef __MY_NEW_OPTION__
  // feature is enabled
#endif
```

**Rules for LPC visibility:**
- Macro must be `#define`d (not `/* #undef */`) in `options.h`
- Macro name must **not** start with `_` (underscore-prefixed macros are skipped by `edit_source`)
- Only boolean (valueless) `#define` declarations are safely portable; value macros become string constants in the predefined table

## Modifying an Existing Option

### Changing a default

Edit **only** the `option()` or `set()` call in [cmake/options.cmake](../../cmake/options.cmake):

```cmake
option(LAZY_RESETS "..." ON)  # was OFF
```

The `options.h.in` template does not encode the default; the CMake variable value flows through `@ONLY` substitution at configure time.

**Important:** Users who have already configured a build have the old default cached in `CMakeCache.txt`. To pick up the new default on an existing build tree, users must delete the build tree or manually set the option.

### Changing a value type (flag → value or vice versa)

Make both files consistent:
- Change `option()` → `set(...CACHE STRING...)` in `options.cmake`
- Change `#cmakedefine NAME` → `#cmakedefine NAME @NAME@` in `options.h.in`

Rebuild from scratch (`cmake --build --preset ci-linux`) to verify.

### Renaming an option

1. Rename in `options.cmake` (update the variable name and description)
2. Rename the `#cmakedefine` in `options.h.in`
3. Update all `#ifdef`/`#ifndef` references in C/C++ source files
4. Search for `__OLD_NAME__` in LPC mudlib code if the macro was LPC-visible

## Unused Options

If an option is defined in `options.cmake` and `options.h.in` but has no `#ifdef` references in source, leave it in place — do not migrate it to `#cmakedefine`. Keeping the `#define` / `#undef` literal preserves the original MudOS behavior as a documentation record and does not affect the build.

## Testing Changes

After modifying any option:

```bash
# Reconfigure to regenerate options.h
cmake --preset linux

# Verify generated options.h has expected defines
grep -E '^(#define|/\* #undef)' out/build/linux/options.h | grep MY_OPTION

# Full build to check compilation
cmake --build --preset ci-linux

# Run unit tests
ctest --preset ut-linux
```

To test with a non-default value:

```bash
cmake --preset linux -DMY_VALUE=99
cmake --build --preset ci-linux
```

### Verifying LPC visibility

After building, inspect `efuns_option.h` to confirm the predefined macro is emitted:

```bash
grep __MY_NEW_OPTION__ out/build/linux/efuns_option.h
```

## Troubleshooting

| Problem | Cause | Fix |
|---------|-------|-----|
| Option not taking effect in C code | `options.h` not regenerated | Reconfigure (`cmake --preset linux`) then rebuild |
| `#define` missing from generated `options.h` | Missing `#cmakedefine` in `options.h.in`, or typo in variable name | Check `options.h.in` matches exactly the cmake variable name |
| LPC can't see `__MY_OPTION__` | Macro is `/* #undef */` (option is OFF) or name starts with `_` | Enable the option in cmake or rename to avoid leading underscore |
| Boolean option always defined regardless of cmake setting | Used `#define NAME` instead of `#cmakedefine NAME` in `options.h.in` | Change to `#cmakedefine` |
| String value option produces `/* #undef MY_PARAM */` | The cmake variable is empty or not set | Provide a default value in `set(MY_PARAM "default" CACHE STRING ...)` |
| Dependent option forced OFF when parent is ON | `cmake_dependent_option` condition string syntax error | Verify condition matches the exact cmake variable name (case-sensitive) |
| `edit_source` not regenerating after change | `edit_source` is only run POST_BUILD on a `--build`, not on configure | Run a build step after `cmake --preset linux` |
| Efun conditionally compiled on option not visible | `func_spec.c.in` uses `#ifdef MY_OPTION` but `MY_OPTION` is not in `options.h` | Ensure `configure_file` processes `options.h.in` before the C preprocessor runs on `func_spec.c`; check build order |

## Key Rules

1. **`options.cmake` is the single source of truth for defaults.** Never hardcode values in `options.h.in`.
2. **`options.h.in` uses `#cmakedefine` only** — no bare `#define`/`#undef` for migrated options.
3. **Names starting with `_` are invisible to LPC** — `edit_source` skips them intentionally.
4. **Changing which efuns are conditionally compiled** (via `#ifdef` blocks in `func_spec.c.in`) shifts opcode numbers. Bump `driver_id` in [lib/lpc/program/binaries.c](../../lib/lpc/program/binaries.c) when this happens.
5. **Do not edit generated files** — `options.h` is written by cmake; `efuns_option.h`, `efuns_opcode.h`, `efuns_vector.h`, `efuns_prototype.h`, `efuns_definition.h` are written by `edit_source`.
