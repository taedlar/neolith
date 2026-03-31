# Plan: LPC to_json / from_json Efuns

Add `to_json(mixed) → string` and `from_json(string) → mixed` efuns backed by Boost.JSON. The efuns are conditionally compiled — they only exist in the driver when the `PACKAGE_JSON` cmake option is `ON` and Boost.JSON is found at configure time (`HAVE_BOOST_JSON`). Implementation lives in `lib/efuns/json.cpp` (C++ translation unit with `extern "C"` efun symbols) to access the Boost.JSON API directly without a separate bridge library.

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Build system | complete |
| 2 | Efun registration | complete |
| 3 | Implementation | complete |
| 4 | Docs | not started |

## Decisions

- **JSON null → LPC**: `T_NUMBER 0` with `subtype = T_UNDEFINED` (reuses existing constant; same as what `undefinedp()` checks)
- **Non-serializable LPC types** (objects, functions, buffers, classes) in `to_json` → emit JSON null
- **Mapping with non-string keys** in `to_json` → `error()`
- **`PACKAGE_JSON` semantics**: a boolean `option()` (default `OFF`); enables JSON efuns when Boost.JSON is available. Consistent with `PACKAGE_SOCKETS`. LPC-visible via `options.h.in`.
- **`HAVE_BOOST_JSON` semantics**: derived cmake variable (not a user option); set when `PACKAGE_JSON` is `ON` and `Boost_json_FOUND`. Declared in `config.h.in` as a detection macro.
- **No backend**: efuns are absent from `func_spec.c` when `PACKAGE_JSON` is `OFF` or Boost.JSON is not found
- **Implementation language**: `lib/efuns/json.cpp` with `extern "C"` for efun symbols

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

New file `lib/efuns/json.cpp`. Include `config.h`, then C driver headers inside `extern "C" { }`, then `<boost/json.hpp>` under `#ifdef HAVE_BOOST_JSON`. Efun bodies inside a second `extern "C" { }` block, guarded by `F_TO_JSON` / `F_FROM_JSON`.

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
- `json_to_lpc(boost::json::value const&, svalue_t*)` — writes into a pre-freed svalue slot; for arrays uses `allocate_array()`, for objects uses `allocate_mapping()` + `insert_in_mapping()`.

## Phase 4: Docs `not started`

*Parallel with Phase 3.*

- **docs/efuns/to_json.md** and **docs/efuns/from_json.md** — follow existing efun doc style. Cover: signature, type conversion table, null round-trip behavior (`T_UNDEFINED` subtype; same as `undefinedp()`), error conditions.
- **docs/ChangeLog.md** — add entry for the two new efuns.

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
7. Docs structurally consistent with adjacent efun docs.

## Further Considerations

1. **Mapping iteration API**: `insert_in_mapping()` handles writes. For reading mapping key/value pairs in `to_json`, need the internal C iteration API — check `lib/lpc/mapping.h` for the correct hash-walk function. Highest-risk unknown.
2. **Boost.JSON header-only fallback**: Boost.JSON 1.75+ is NOT header-only; it requires linking `libboost_json`. The `BOOST_JSON_HEADER_ONLY` detection path in `CMakeLists.txt` (for old Boost) is retained but will fail at configure if `boost/json.hpp` is absent. In practice, install `libboost-json-dev` (e.g. `apt install libboost-json1.83-dev`). The CMakeLists.txt also detects `Boost_json_FOUND` correctly once the compiled library is installed via `find_package(Boost OPTIONAL_COMPONENTS json)`.
3. **longjmp + C++ destructors**: Pre-validation in `f_to_json` prevents `error()` from being called mid-Boost-allocation. For `from_json`, the error-code parse overload avoids Boost-side exceptions; LPC allocation errors in `json_to_lpc()` are catastrophic-context and treated as unrecoverable. Document this limitation in the efun docs.
