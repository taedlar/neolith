# Plan: LPC to_json / from_json Efuns

Add `to_json(mixed) → string` and `from_json(string) → mixed` efuns backed by Boost.JSON. The efuns are conditionally compiled — they only exist in the driver when the `PACKAGE_JSON` cmake option is `ON` and Boost.JSON is found at configure time (`HAVE_BOOST_JSON`). Implementation lives in `lib/efuns/json.cpp` (C++ translation unit with `extern "C"` efun symbols) to access the Boost.JSON API directly without a separate bridge library.

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Build system | complete |
| 2 | Efun registration | complete |
| 3 | Implementation | complete |
| 3b | Unit tests | complete |
| 4 | Docs | complete |

## Current State Handoff

- JSON efuns are complete: build gating, efun registration, implementation, unit tests, and documentation are all in place.
- Availability is controlled by `PACKAGE_JSON`; when it is `OFF`, `to_json()` and `from_json()` are absent from generated efun tables.
- Unit coverage lives in `tests/test_efuns/test_json.cpp` and is compiled unconditionally; test bodies are gated by generated `F_TO_JSON` / `F_FROM_JSON` macros.
- User-facing docs now live in `docs/efuns/to_json.md` and `docs/efuns/from_json.md`.

## Decisions

- **JSON null → LPC**: `T_NUMBER 0` with `subtype = T_UNDEFINED` (reuses existing constant; same as what `undefinedp()` checks)
- **Non-serializable LPC types** (objects, functions, buffers, classes) in `to_json` → emit JSON null
- **Mapping with non-string keys** in `to_json` → `error()`
- **`PACKAGE_JSON` semantics**: a boolean `option()` (default `OFF`); enables JSON efuns when Boost.JSON is available. Consistent with `PACKAGE_SOCKETS`. LPC-visible via `options.h.in`.
- **`HAVE_BOOST_JSON` semantics**: derived cmake variable (not a user option); set when `PACKAGE_JSON` is `ON` and `Boost_json_FOUND`. Declared in `config.h.in` as a detection macro.
- **No backend**: efuns are absent from generated efun tables when `PACKAGE_JSON` is `OFF` or Boost.JSON is not found
- **Implementation language**: `lib/efuns/json.cpp` with `extern "C"` for efun symbols
- **Float text form**: `to_json()` preserves numeric value, but the exact JSON number spelling comes from Boost.JSON and may use scientific notation (for example `1.5E0`)

## Type Conversion

**LPC → JSON (`to_json`)**

| LPC type | JSON output |
|----------|-------------|
| T_NUMBER (subtype 0) | integer |
| T_NUMBER (subtype T_UNDEFINED) | null |
| T_REAL | double |
| T_STRING | string |
| T_ARRAY | array (recursive) |
| T_MAPPING (string keys) | object (recursive values) |
| T_MAPPING (non-string key) | error() |
| T_OBJECT / T_FUNCTION / T_BUFFER / T_CLASS | null |

**JSON → LPC (`from_json`)**

| JSON kind | LPC result |
|-----------|------------|
| integer (int64/uint64) | T_NUMBER subtype 0 |
| double | T_REAL |
| string | T_STRING STRING_MALLOC |
| array | T_ARRAY (recursive) |
| object | T_MAPPING (string keys, recursive values) |
| bool | T_NUMBER 0/1 subtype 0 |
| null | T_NUMBER 0 subtype T_UNDEFINED |

## Phase 1: Build System `complete`

Steps are independent of each other.

- **cmake/options.cmake**: Add in the MudOS options section (near `PACKAGE_SOCKETS`):
  ```cmake
  option(PACKAGE_JSON "Enable JSON efuns to_json() and from_json() (requires Boost.JSON)" OFF)
  ```

- **lib/lpc/options.h.in**: Add `#cmakedefine PACKAGE_JSON` in the Miscellaneous section (near other `PACKAGE_*` entries) so LPC code can test `#ifdef __PACKAGE_JSON__`:
  ```c
  /* PACKAGE_JSON: Enables to_json() and from_json() efuns.
   * Requires Boost.JSON at build time (set PACKAGE_JSON=ON and have Boost with json component).
   */
  #cmakedefine PACKAGE_JSON
  ```

- **CMakeLists.txt**: Change `find_package(Boost)` → `find_package(Boost OPTIONAL_COMPONENTS json)` so the json component is discovered without being required. After the existing `Boost_FOUND` block, add:
  ```cmake
  if(PACKAGE_JSON)
      if(NOT Boost_json_FOUND)
          message(FATAL_ERROR "PACKAGE_JSON requires Boost.JSON (Boost::json component not found)")
      endif()
      set(HAVE_BOOST_JSON 1)
      message(STATUS "PACKAGE_JSON enabled: JSON efuns will use Boost.JSON")
  endif()
  ```

- **config.h.in**: Add `#cmakedefine HAVE_BOOST_JSON` immediately after `#cmakedefine HAVE_BOOST`.

- **lib/efuns/CMakeLists.txt**: After the `add_library(efuns ...)` block, conditionally compile `json.cpp` and link `Boost::json`:
  ```cmake
  if(HAVE_BOOST_JSON)
      target_sources(efuns PRIVATE json.cpp)
      target_link_libraries(efuns PUBLIC Boost::json)
  endif()
  ```
  Note: `CPP_COMMAND` in `lib/lpc/CMakeLists.txt` already passes `-I ${CMAKE_BINARY_DIR}` where `config.h` lives, so `#ifdef HAVE_BOOST_JSON` in `func_spec.c` will resolve correctly without changes.

## Phase 2: Efun Registration `complete`

*Depends on Phase 1.*

- **lib/lpc/func_spec.c.in**: Add signatures in the data-conversion group (near `to_int`, `to_float`), guarded by `#ifdef PACKAGE_JSON`:
  ```c
  #ifdef PACKAGE_JSON
  string to_json(mixed);
  mixed from_json(string);
  #endif
  ```

## Phase 3: Implementation `complete`

*Depends on Phases 1–2.*

New file `lib/efuns/json.cpp`. Include `config.h`, then C driver headers, then `<boost/json.hpp>` under `#ifdef HAVE_BOOST_JSON`. Efun bodies inside a `extern "C" { }` block, guarded by `F_TO_JSON` / `F_FROM_JSON`.

**`f_to_json`** (1 arg at `sp`):
- Pre-validate all mapping keys before allocating any Boost.JSON objects (prevents longjmp bypassing C++ stack cleanup).
- Convert the svalue tree → `boost::json::value` via recursive file-static helper `lpc_to_json()`.
- Serialize with `boost::json::serialize()`, free original `sp` svalue, write result as `STRING_MALLOC` string into `sp`.

**`f_from_json`** (1 arg — string at `sp`):
- Parse using the error-code overload (`boost::json::parse(input, ec)`) — no exceptions.
- On parse error: free string, reset slot to `T_NUMBER 0`, then `error()`.
- On success: free string, write result into `sp` via recursive file-static helper `json_to_lpc()`.

**Recursive helpers** (file-static, not exported):
- `lpc_to_json(svalue_t*)` → `boost::json::value` — maps all LPC types per conversion table above.
- `json_to_lpc(boost::json::value const&, svalue_t*)` — writes into a pre-freed svalue slot; for arrays uses `allocate_array()`, for objects uses `allocate_mapping()` + `find_for_insert()`.

## Phase 3b: Unit Tests `complete`

*Depends on Phase 3. Tests live in `tests/test_efuns/test_json.cpp`, compiled as part of the `test_efuns` target.*

New file `tests/test_efuns/test_json.cpp`. All test bodies are guarded by `#ifdef F_TO_JSON` / `#ifdef F_FROM_JSON` so the file compiles (with empty test suite) when `PACKAGE_JSON=OFF`.

**`to_json` tests** (guarded by `#ifdef F_TO_JSON`):
- `toJsonInteger` / `toJsonNegativeInteger` — integer → `"42"` / `"-7"`
- `toJsonFloat` — float 1.5 → JSON numeric text (currently `"1.5E0"` with Boost.JSON)
- `toJsonString` — `"hello"` → `'"hello"'`
- `toJsonUndefined` — undefined (subtype `T_UNDEFINED`) → `"null"`
- `toJsonArray` — array `[1,2,3]` → `"[1,2,3]"`
- `toJsonMapping` — single-key mapping `{"k":99}` → `'{"k":99}'`
- `toJsonNonStringKeyError` — mapping with integer key → `error()` via `setjmp`/`restore_context`

**`from_json` tests** (guarded by `#ifdef F_FROM_JSON`):
- `fromJsonInteger` / `fromJsonNegativeInteger` — `"42"` / `"-7"` → `T_NUMBER`
- `fromJsonFloat` — `"1.5"` → `T_REAL`
- `fromJsonString` — `'"hello"'` → `T_STRING "hello"`
- `fromJsonNull` — `"null"` → `T_NUMBER 0` subtype `T_UNDEFINED`
- `fromJsonBoolTrue` / `fromJsonBoolFalse` — `"true"` / `"false"` → `T_NUMBER 1` / `0`
- `fromJsonArray` — `"[1,2,3]"` → `T_ARRAY` size 3 with correct elements
- `fromJsonObject` — `'{"a":1}'` → `T_MAPPING` with key `"a"` → 1
- `fromJsonInvalidError` — `"{ invalid"` → `error()` via `setjmp`/`restore_context`

**Round-trip tests** (guarded by `#if defined(F_TO_JSON) && defined(F_FROM_JSON)`):
- `roundTripInteger` — `from_json(to_json(12345))` → `T_NUMBER 12345`
- `roundTripString` — `from_json(to_json("hello world"))` → `T_STRING "hello world"`
- `roundTripNull` — `to_json(undefined)` → `"null"`; `from_json("null")` has subtype `T_UNDEFINED`
- `roundTripArray` — array `[10, 20]` round-trips with correct element values
- `roundTripFloat` — float round-trips by numeric value regardless of serialized spelling

**CMake**: add `test_json.cpp` to `test_efuns` target (unconditionally; `#ifdef` guards handle the disabled case).

## Phase 4: Docs `complete`

- Added `docs/efuns/to_json.md` with availability, conversion, and error semantics for `to_json()`.
- Added `docs/efuns/from_json.md` with JSON-to-LPC conversion rules and parse-failure behavior for `from_json()`.
- Updated `docs/ChangeLog.md` with unreleased feature and documentation notes for JSON efuns.

## Relevant Files

| File | Change |
|------|--------|
| `cmake/options.cmake` | `PACKAGE_JSON` boolean option declaration |
| `lib/lpc/options.h.in` | `#cmakedefine PACKAGE_JSON` for LPC visibility |
| `CMakeLists.txt` | Boost `OPTIONAL_COMPONENTS json`, `HAVE_BOOST_JSON` derived var |
| `config.h.in` | `#cmakedefine HAVE_BOOST_JSON` |
| `lib/lpc/func_spec.c.in` | Guarded efun signatures |
| `lib/efuns/CMakeLists.txt` | `json.cpp` in sources, conditional `Boost::json` link |
| `lib/efuns/json.cpp` | New file — implementation |
| `tests/test_efuns/test_json.cpp` | New unit tests for JSON efuns |
| `docs/efuns/to_json.md` | New efun doc |
| `docs/efuns/from_json.md` | New efun doc |
| `docs/ChangeLog.md` | Changelog entry |

## Verification

1. Build with Boost: `F_TO_JSON` / `F_FROM_JSON` appear in generated `efuns_opcode.h`.
2. Build without Boost: those symbols absent from all generated headers.
3. LPC round-trip: `from_json(to_json(x)) == x` for int, float, string, nested array, nested mapping.
4. LPC null: `undefinedp(from_json("null"))` is true; `to_json(from_json("null"))` produces `"null"`.
5. LPC error: `to_json` on a mapping with integer key → error via `catch()`.
6. LPC error: `from_json("{ invalid")` → controlled error via `catch()`.
7. Unit tests pass in `test_efuns` with JSON enabled.
8. Docs structurally consistent with adjacent efun docs.

## Lessons Learned

- Boost.JSON may choose scientific notation for floating-point output, so tests and docs should assert numeric behavior, not a hand-written decimal spelling unless the exact backend format is part of the contract.
- `find_for_insert()` is the practical mapping-construction helper for tests and conversion code, but it mutates string-key ownership semantics through shared-string interning; tests must release the extra shared-string reference they create.
- When efuns are build-gated, the cleanest test strategy is to always compile the test file and gate the individual test bodies with generated `F_*` macros instead of adding more CMake conditionals.
- **Boost MSVC autolink vs CMake targets (post-close fix, 2026-03-31)**: On MSVC, Boost headers emit `#pragma comment(lib, ...)` to autolink the implementation. The pragma encodes the lib variant suffix (e.g. `-s-` for static CRT) based on compiler defines at point of inclusion, independent of CMake's `Boost::json` target. If `Boost_USE_STATIC_RUNTIME` is not set before `find_package(Boost)`, CMake resolves `Boost::json` to a different variant than the pragma expects, causing LNK1104. Fix: set `Boost_USE_STATIC_RUNTIME ON/OFF` before `find_package` conditioned on `USE_STATIC_MSVC_RUNTIME`, and define `BOOST_ALL_NO_LIB` (scoped to `if(MSVC)`) to disable the pragma so CMake is the sole link mechanism. `BOOST_ALL_NO_LIB` is a no-op on Linux/GCC/Clang where autolink does not exist.

## Remaining Work

- None for this feature. Archive or close the plan only when requested.
