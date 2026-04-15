# Counted and Shared String Length Representation

## Summary

### Overview

This plan moves LPC runtime strings from implicit C-string assumptions to
explicit byte-span semantics while preserving LPC compatibility.

### Foundation Completed

- `blkend` now stores long-string end-of-span metadata, keeping logical length
  O(1) at sentinel-size boundaries.
- Shared strings are capped below `USHRT_MAX`, reserving sentinel
  `size == USHRT_MAX` for malloc-string long-length handling.
- Shared intern/find operations are span-based (`make_shared_string(s,end)`,
  `findstring(s,end)`) and no longer require temporary NUL copies.
- Allocation paths still append a trailing `'\0'` outside the logical span.
  This preserves LPC string contract compatibility (strings are NUL-terminated;
  non-terminated raw bytes belong to `buffer`) and protects efun/driver
  boundaries such as `from_json` and `read_buffer` when downstream C code reads
  payloads as C strings.

### Safety Goal and Current Gap

Goal: force explicit intent at typed string boundaries and block misuse as
early as possible (compile-time preferred, runtime as backstop).

| Mode | Compile-time | Runtime |
|---|---|---|
| `OFF` | ✗ shared/malloc domain mixups usually compile | ✗ no added boundary checks |
| `ON` (current) | ⚠ intent visible in signatures, mixups still compile | ✓ contract violations fail fast in release builds |
| `ON` (target) | ✓ mixups blocked at call site (explicit conversion required) | ✓ runtime checks remain secondary defense |

### Near-Term Focus

- Promote transparent boundary handles to abstract handles so boundary misuse fails at
  compile time.
- Harden efun and JSON byte-span boundaries with explicit contract checks.
- Expand regression coverage to lock in counted-string semantics across LPC,
  efuns, and JSON boundaries.

## Status

| Stage | Status |
|---|---|
| Foundation: `blkend` model + shared-table non-NUL lookup + initial safety/tests | complete |
| Implementation: VM/operator NUL-removal and span API normalization | complete |
| Implementation: abstract typed handles + runtime contract enforcement | in progress |
| Implementation: C++ RAII wrapper adoption on exception baseline | not started |
| Implementation: efun byte-span readiness | not started |
| Implementation: JSON boundary contract and tests | in progress |
| Validation: end-to-end LPC/JSON regression matrix | in progress |

## Current State Handoff

As of 2026-04-15:

### Completed Scope

- Core counted/shared length representation changes are complete and tested.
- P0 VM/operator NUL-removal and span migration is complete in
  `src/interpret.h` and `lib/lpc/operator.c`.
- Unit coverage for string operators is in `tests/test_string_operators`, with
  discovery via `gtest_discover_tests()`.

### Active Focus

- Planning scope is narrowed to one consolidated backlog with acceptance criteria.
- Current implementation focus: typed-handle enforcement, C++ wrapper adoption
  on existing exception boundaries, and efun/JSON boundary hardening.
- Safe transition is underway: boundary-handle enforcement is live in
  `stralloc`, and subtype-known write sites are being migrated from generic
  `u.string` assignments to typed `u.shared_string` / `u.malloc_string`
  members without changing the C ABI.
- Compile-time regression checks for typed boundary signatures and bridge
  helpers are now in `tests/test_stralloc/test_stralloc_type_safety.cpp`.
- `src` low-risk subtype-known writes are now clean in the latest sweep:
  `src/stralloc.c`, `src/outbuf.c`, and `src/error_context.cpp` now use
  typed members (`u.shared_string` / `u.malloc_string` / `u.const_string`)
  at subtype-known assignment sites.

### Baseline and Out of Scope

- Exception migration (`setjmp`/`longjmp` retirement and C++ guard boundaries)
  is completed baseline and not part of this plan's remaining implementation scope.
- `alloc_cstring` remains intentionally outside counted-string semantics.

## Design Constraints (Canonical)

- LPC runtime string semantics are byte-sequence semantics, not JSON text semantics.
- JSON semantics are enforced only at JSON boundaries.
- `from_json` validates UTF-8 at runtime before constructing LPC strings and
  raises an LPC runtime error when input contains invalid UTF-8.
- Identifier-class shared strings remain NUL-terminated by contract.
- This includes function names, variable names, and predefines.
- `u.string` remains available as a generic view; typed members supplement it.
- When migrating functions or macros that process `svalue_t`, always validate all runtime string semantics: `STRING_MALLOC`, `STRING_SHARED`, `STRING_CONSTANT`.

## UTF-8 Compatibility Contract

- Counted-string storage is byte-oriented, not Unicode-scalar-oriented.
  Driver-level and LPC-level length semantics count bytes, not UTF-8 characters.
- Counted strings are not globally required to be valid UTF-8.
  Invalid UTF-8 byte sequences may exist in LPC strings unless an operation
  explicitly requires UTF-8 validity.
- UTF-8 validity is enforced at API boundaries that require text semantics.
  `from_json` rejects invalid UTF-8 and raises an LPC runtime error before
  producing an LPC string. Other efuns with text/character semantics (for
  example `explode`) may also reject invalid UTF-8 and raise runtime errors
  for that operation.
- UTF-8 character counting via `explode` is an operation-specific result and
  must not be treated as LPC string length.
  LPC string length and driver counted-string length remain byte counts.

## Counted-String Contract and Type Safety

Type/source of truth for counted-string contract:

1. `svalue_t.subtype` for generic runtime paths.
2. Function boundary contract for typed pointer parameters.

Contract-bearing boundary functions:

| Function | Contract |
|---|---|
| `ref_string(shared_str_handle_t)` / `free_string(shared_str_handle_t)` | requires shared payload |
| `extend_string(malloc_str_handle_t, size_t)` | requires malloc payload |
| `push_shared_string(shared_str_t)` / `push_malloced_string(malloc_str_t)` | subtype-specific storage boundaries |

These boundary signatures are now type-specific end to end. Remaining hardening
moves enforcement earlier: abstract-handle mode makes cross-domain misuse a
compile error; expanded typed-member coverage closes gaps where intent is
currently only annotated, not enforced.

`STRING_TYPE_SAFETY` layers:

- Layer 1: typed aliases signal intent at call sites — misuse is visible in
  signatures but not yet blocked by the compiler.
- Layer 2: runtime contract checks block misuse that survives compile time —
  boundary violations are fatal in release builds.

## Planned Abstract Handle Migration

Target: when `STRING_TYPE_SAFETY` is enabled, switch boundary handles from
transparent pointer aliases to abstract handle typedefs plus explicit bridge
helpers — making cross-domain misuse a compile error rather than a runtime
surprise.

```c
#ifdef STRING_TYPE_SAFETY
  typedef struct { char *raw; } shared_str_handle_t;
  typedef struct { char *raw; } malloc_str_handle_t;
  #define SHARED_STR_P(x) ((x).raw)
  #define MALLOC_STR_P(x) ((x).raw)
  static inline shared_str_handle_t to_shared_str(char *p) {
    shared_str_handle_t h = { p };
    return h;
  }
  static inline malloc_str_handle_t to_malloc_str(char *p) {
    malloc_str_handle_t h = { p };
    return h;
  }
#else
  typedef char *shared_str_handle_t;
  typedef char *malloc_str_handle_t;
  #define SHARED_STR_P(x) (x)
  #define MALLOC_STR_P(x) (x)
  #define to_shared_str(x) (x)
  #define to_malloc_str(x) (x)
#endif
```

Migration order:

1. `stralloc.h`/`stralloc.c` handle typedefs + bridge helpers/macros.
2. Add typed members in `svalue_u` alongside `u.string`.
3. Convert subtype-specific storage boundaries first.
4. Expand to efun write sites where subtype is known.
5. Add compile-time layout/offset checks for `svalue_t`/`svalue_u` where typed
   members are introduced.
6. Leave generic read-only paths on `u.string` until separate NUL-path migration.

## C++ Wrapper Adoption on Exception Baseline

- Keep `svalue_t`/`svalue_u` as C-layout POD in C-visible headers.
- Add C++ wrappers:
- `lpc::svalue_view` (non-owning view over `svalue_t *`) and `lpc::svalue` (RAII owner).
- Exception transport and `extern "C"` boundary translation are existing baseline
  behavior from the completed exception migration; this plan must not re-open
  or duplicate that work.
- Move/dtor paths for wrappers must be `noexcept`.
- `BorrowedValue` must be allocation-free in hot interpreter paths.

## Consolidated Backlog with Acceptance Criteria

| Priority | Item | Scope | Acceptance criteria |
|---|---|---|---|
| P0 | Remove NUL-dependent VM/operator paths | [lib/lpc/operator.c](../../lib/lpc/operator.c), [src/interpret.h](../../src/interpret.h) | complete: touched concat/join paths now use explicit lengths and byte-copy semantics; equality/inequality and string range use counted-length compare/copy; behavior remains compatible for normal strings. |
| P0 | Normalize internal helper APIs | string construction/lookup boundaries | New/updated helpers accept explicit lengths/spans; touched callers stop using sentinel termination as logical length; shared-string boundaries continue to route via `make_shared_string(s,end)` / `findstring(s,end)`. |
| P0 | Enforce counted-string semantic boundaries | [src/stralloc.h](../../src/stralloc.h), [lib/lpc/types.h](../../lib/lpc/types.h), typed-string boundaries | Boundary-handle mode enabled under `STRING_TYPE_SAFETY`; contract APIs require explicit typed handles or bridge helpers; runtime contract checks remain release-enabled; identifier-class shared strings remain NUL-terminated. |
| P1 | C++ wrapper adoption on baseline boundaries | C++ wrappers around `svalue_t` | `lpc::svalue_view`/`lpc::svalue` introduced without C ABI layout change; no duplicate exception-boundary rewrites are introduced; wrapper move/dtor are `noexcept`; targeted perf checks show no hot-path regression. |
| P1 | Efun byte-span readiness | [lib/efuns/string.c](../../lib/efuns/string.c), [lib/efuns/unsorted.c](../../lib/efuns/unsorted.c), [lib/efuns/sprintf.c](../../lib/efuns/sprintf.c), [lib/efuns/sscanf.c](../../lib/efuns/sscanf.c) | Touched binary-sensitive efun paths use explicit lengths; text-oriented paths explicitly document C-string assumptions; existing efun tests remain green. |
| P1 | JSON boundary contract | JSON efuns/helpers (`from_json`, `to_json`) | Contract docs state LPC byte spans vs JSON text; `from_json` rejects invalid UTF-8 and raises LPC runtime error on invalid sequences; `to_json` escaping policy documented and tested. |
| P1 | Unicode and escape consistency | JSON encode/decode implementation | Encoder/decoder are symmetric for control escapes, `\\`, `\"`, `\uXXXX`, and surrogate pairs; non-BMP behavior documented and validated. |
| P2 | End-to-end regression matrix | LPC-level and efun/unit tests | in progress: dedicated unit suite `tests/test_string_operators` added (21 cases, discovered via CTest); remaining coverage is LPC/JSON round-trip and negative matrix expansion. Any wrapper/ABI-adjacent change must pass `ut-linux`, `ut-vs16-x64`, and `ut-clang-x64` before stage completion. |

## Hardening Gates for Remaining Work

1. Exception migration remains a prerequisite baseline; counted-string work must
  consume the established exception boundaries and avoid reopening boundary
  transport changes.
2. Any change that adds typed members or wrapper views around `svalue_t` must
  include explicit layout/ABI checks and document compatibility impact.
3. Any C++ wrapper adoption change touching mixed C/C++ boundaries must be
  validated on Linux, MSVC x64, and clang-cl x64 test presets before closure.
4. Regression additions should prioritize behavior contracts over source-shape
  assertions to reduce brittleness during incremental refactors.

## Remaining NUL-Dependent Paths (Index)

| Area | Location | Note |
|---|---|---|
| VM string ops | [src/interpret.h](../../src/interpret.h) (`EXTEND_SVALUE_STRING`, `SVALUE_STRING_ADD_LEFT`, `SVALUE_STRING_JOIN`) | primary paths migrated to explicit-length macros; compatibility wrappers still use `strlen` for NUL-terminated call sites |
| Generic length macro | [src/stralloc.h](../../src/stralloc.h) (`SVALUE_STRLEN`) | still falls back to `strlen` for `STRING_CONSTANT` |
| String efuns | [lib/efuns/string.c](../../lib/efuns/string.c), [lib/efuns/unsorted.c](../../lib/efuns/unsorted.c), [lib/efuns/sprintf.c](../../lib/efuns/sprintf.c), [lib/efuns/sscanf.c](../../lib/efuns/sscanf.c) | text/C-string assumptions remain in several paths |
| Driver internals | [src/comm.c](../../src/comm.c), [lib/lpc/mapping.c](../../lib/lpc/mapping.c) | output/restore paths still depend on C-string-oriented behavior |
| Intentional contract | [src/apply.c](../../src/apply.c), [lib/efuns/replace_program.c](../../lib/efuns/replace_program.c) | identifier/text contracts intentionally remain NUL-terminated |

## Lessons Learned

- Enforcing shared-size cap at allocation boundaries is sufficient.
- Canonicalizing oversized shared keys before hash lookup is required for dedupe.
- `COUNTED_STRLEN` fallback for legacy `blkend == NULL` remains necessary until
  binary-format bump.
- `SVALUE_STRING_JOIN` consumes/frees the right operand but leaves the left
  operand owning the joined result; tests must explicitly free the left value.
- `SVALUE_STRLEN_DIFFERS` using only `MSTR_SIZE` is conservative for sentinel
  long malloc strings (`size == USHRT_MAX`); exact long-string mismatch
  fast-paths should consult counted logical length (`COUNTED_STRLEN`).
- Transparent aliases are a staging step; runtime checks block misuse until
  abstract handles make explicit intent mandatory at compile time.
