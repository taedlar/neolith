# Plan: LPC to_json / from_json Efuns

Add `to_json(mixed) → string` and `from_json(string) → mixed` efuns backed by Boost.JSON. The efuns are conditionally compiled — they only exist in the driver when `HAVE_BOOST_JSON` is detected at cmake time and no `PACKAGE_JSON` cmake option overrides the backend. Implementation lives in `lib/efuns/json.cpp` (C++ translation unit with `extern "C"` efun symbols) to access the Boost.JSON API directly without a separate bridge library.

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Build system | not started |
| 2 | Efun registration | not started |
| 3 | Implementation | not started |
| 4 | Docs | not started |

## Decisions

- **JSON null → LPC**: `T_NUMBER 0` with `subtype = T_UNDEFINED` (reuses existing constant; same as what `undefinedp()` checks)
- **Non-serializable LPC types** (objects, functions, buffers, classes) in `to_json` → emit JSON null
- **Mapping with non-string keys** in `to_json` → `error()`
- **No backend**: efuns are absent from `func_spec.c` when no JSON backend is configured
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

## Phase 1: Build System `not started`

Steps are independent of each other.

- **CMakeLists.txt**: Change `find_package(Boost)` → `find_package(Boost COMPONENTS json)`. Add `PACKAGE_JSON` cmake cache string option (empty default). Set `HAVE_BOOST_JSON=1` if `Boost_json_FOUND AND NOT PACKAGE_JSON`.
- **config.h.in**: Add `#cmakedefine HAVE_BOOST_JSON` after `#cmakedefine HAVE_BOOST`.
- **lib/efuns/CMakeLists.txt**: Append `json.cpp` to `efuns_SOURCES` and link `Boost::json` conditionally on `HAVE_BOOST_JSON AND NOT PACKAGE_JSON`. Verify `CPP_COMMAND` used for `func_spec.c` preprocessing includes the cmake binary directory (where `config.h` lives) so `#ifdef HAVE_BOOST_JSON` resolves; add `-I${CMAKE_BINARY_DIR}` if missing.

## Phase 2: Efun Registration `not started`

*Depends on Phase 1 (CPP_COMMAND include fix).*

- **lib/efuns/func_spec.c**: Add signatures in the data-conversion group (near `to_int`, `to_float`), guarded by `#ifdef HAVE_BOOST_JSON`:
  ```c
  #ifdef HAVE_BOOST_JSON
  string to_json(mixed);
  mixed from_json(string);
  #endif
  ```

## Phase 3: Implementation `not started`

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
| `CMakeLists.txt` | Boost COMPONENTS json, `HAVE_BOOST_JSON`, `PACKAGE_JSON` option |
| `config.h.in` | `#cmakedefine HAVE_BOOST_JSON` |
| `lib/efuns/func_spec.c` | Guarded efun signatures |
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
2. **Boost.JSON header-only fallback**: If the installed Boost does not provide a compiled `Boost::json` cmake target, add `#define BOOST_JSON_HEADER_ONLY` before the include and drop the `target_link_libraries` call.
3. **longjmp + C++ destructors**: Pre-validation in `f_to_json` prevents `error()` from being called mid-Boost-allocation. For `from_json`, the error-code parse overload avoids Boost-side exceptions; LPC allocation errors in `json_to_lpc()` are catastrophic-context and treated as unrecoverable. Document this limitation in the efun docs.
