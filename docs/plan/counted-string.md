# Counted and Shared String Length Representation

## Summary

This plan tracks migration from implicit C-string behavior to explicit byte-span
semantics for LPC runtime strings while preserving compatibility constraints.

Completed foundation work:

- `malloc_block_t.unused` was replaced with `blkend`, enabling O(1)
  `COUNTED_STRLEN` even when `size == USHRT_MAX`.
- `STRING_SHARED` allocation is capped at `USHRT_MAX - 1`, so sentinel
  `size == USHRT_MAX` remains exclusive to `STRING_MALLOC`.
- Shared-string table lookup/insert is length-aware (`StrHashN`, `findblockn`,
  `make_shared_string(s,end)`, `findstring(s,end)`), so intern/find no longer
  depends on temporary NUL-terminated copies.

Remaining work is implementation and validation focused on typed counted-string
boundaries, efun byte-span hardening, JSON boundary behavior, and expanded
regression coverage.

## Status

| Stage | Status |
|---|---|
| Foundation: `blkend` model + shared-table non-NUL lookup + initial safety/tests | complete |
| Implementation: VM/operator NUL-removal and span API normalization | complete |
| Implementation: abstract typed handles + runtime contract enforcement | not started |
| Implementation: C++ RAII wrapper + exception boundary migration | not started |
| Implementation: efun byte-span readiness | not started |
| Implementation: JSON boundary contract and tests | in progress |
| Validation: end-to-end LPC/JSON regression matrix | in progress |

## Current State Handoff

As of 2026-04-05:

- Core counted/shared length representation changes are complete and tested.
- Planning scope is now narrowed to one consolidated backlog with acceptance criteria.
- P0 VM/operator NUL-removal and span migration is complete in
  `src/interpret.h` and `lib/lpc/operator.c`.
- New unit coverage for string operators is in `tests/test_string_operators`,
  and discovery now uses `gtest_discover_tests()`.
- Current implementation focus should move to typed-handle enforcement and
  efun/json boundary hardening.
- `alloc_cstring` remains intentionally outside counted-string semantics.

## Design Constraints (Canonical)

- LPC runtime string semantics are byte-sequence semantics, not JSON text semantics.
- JSON semantics are enforced only at JSON boundaries.
- `from_json` validates UTF-8 at runtime before constructing LPC strings.
- Identifier-class shared strings remain NUL-terminated by contract.
- This includes function names, variable names, and predefines.
- `u.string` remains available as a generic view; typed members supplement it.
- When migrating functions or macros that process `svalue_t`, always validate all runtime string semantics: `STRING_MALLOC`, `STRING_SHARED`, `STRING_CONSTANT`.

## Counted-String Contract and Type Safety

Type/source of truth for counted-string contract:

1. `svalue_t.subtype` for generic runtime paths.
2. Function boundary contract for typed pointer parameters.

Contract-bearing boundary functions:

| Function | Contract |
|---|---|
| `ref_string(shared_str_t)` / `free_string(shared_str_t)` | requires shared payload |
| `extend_string(malloc_str_t, size_t)` | requires malloc payload |
| `push_shared_string(shared_str_t)` / `push_malloced_string(malloc_str_t)` | subtype-specific storage boundaries |

These boundary signatures are now type-specific end to end; the remaining hardening
work is abstract-handle mode and expanded typed-member write coverage.

`STRING_TYPE_SAFETY` behavior:

- Layer 1: typed aliases in signatures (`shared_str_t`, `malloc_str_t`).
- Layer 2: always-on runtime contract checks for `extend_string` and
  `string_unlink` in release builds.

## Planned Abstract Handle Migration

Target: when `STRING_TYPE_SAFETY` is enabled, switch from transparent aliases to
abstract handle typedefs plus explicit bridge macros.

```c
#ifdef STRING_TYPE_SAFETY
  typedef struct { char *raw; } shared_str_t;
  typedef struct { char *raw; } malloc_str_t;
  #define SHARED_STR_P(x)  ((x).raw)
  #define MALLOC_STR_P(x)  ((x).raw)
  #define TO_SHARED_STR(x) ((shared_str_t){ .raw = (x) })
  #define TO_MALLOC_STR(x) ((malloc_str_t){ .raw = (x) })
#else
  typedef char *shared_str_t;
  typedef char *malloc_str_t;
  #define SHARED_STR_P(x)  (x)
  #define MALLOC_STR_P(x)  (x)
  #define TO_SHARED_STR(x) (x)
  #define TO_MALLOC_STR(x) (x)
#endif
```

Migration order:

1. `stralloc.h`/`stralloc.c` handle typedef + macros.
2. Add typed members in `svalue_u` alongside `u.string`.
3. Convert subtype-specific storage boundaries first.
4. Expand to efun write sites where subtype is known.
5. Leave generic read-only paths on `u.string` until separate NUL-path migration.

## C++ Wrapper and Exception Migration

- Keep `svalue_t`/`svalue_u` as C-layout POD in C-visible headers.
- Add C++ wrappers:
- `BorrowedValue` (non-owning view over `svalue_t *`) and `OwnedValue` (RAII owner).
- No exception may cross `extern "C"` boundaries.
- Move/dtor paths for wrappers must be `noexcept`.
- `BorrowedValue` must be allocation-free in hot interpreter paths.

## Consolidated Backlog with Acceptance Criteria

| Priority | Item | Scope | Acceptance criteria |
|---|---|---|---|
| P0 | Remove NUL-dependent VM/operator paths | [lib/lpc/operator.c](../../lib/lpc/operator.c), [src/interpret.h](../../src/interpret.h) | complete: touched concat/join paths now use explicit lengths and byte-copy semantics; equality/inequality and string range use counted-length compare/copy; behavior remains compatible for normal strings. |
| P0 | Normalize internal helper APIs | string construction/lookup boundaries | New/updated helpers accept explicit lengths/spans; touched callers stop using sentinel termination as logical length; shared-string boundaries continue to route via `make_shared_string(s,end)` / `findstring(s,end)`. |
| P0 | Enforce counted-string semantic boundaries | [src/stralloc.h](../../src/stralloc.h), [lib/lpc/types.h](../../lib/lpc/types.h), typed-string boundaries | Abstract handle mode enabled under `STRING_TYPE_SAFETY`; boundary APIs require explicit typed handles or bridge macros; runtime contract checks remain release-enabled; identifier-class shared strings remain NUL-terminated. |
| P1 | C++ wrapper and exception boundaries | C++ boundaries around `svalue_t` | `BorrowedValue`/`OwnedValue` introduced without C ABI layout change; all `extern "C"` entry points catch/translate exceptions; wrapper move/dtor are `noexcept`; targeted perf checks show no hot-path regression. |
| P1 | Efun byte-span readiness | [lib/efuns/string.c](../../lib/efuns/string.c), [lib/efuns/unsorted.c](../../lib/efuns/unsorted.c), [lib/efuns/sprintf.c](../../lib/efuns/sprintf.c), [lib/efuns/sscanf.c](../../lib/efuns/sscanf.c) | Touched binary-sensitive efun paths use explicit lengths; text-oriented paths explicitly document C-string assumptions; existing efun tests remain green. |
| P1 | JSON boundary contract | JSON efuns/helpers (`from_json`, `to_json`) | Contract docs state LPC byte spans vs JSON text; `from_json` rejects invalid UTF-8 consistently; `to_json` escaping policy documented and tested. |
| P1 | Unicode and escape consistency | JSON encode/decode implementation | Encoder/decoder are symmetric for control escapes, `\\`, `\"`, `\uXXXX`, and surrogate pairs; non-BMP behavior documented and validated. |
| P2 | End-to-end regression matrix | LPC-level and efun/unit tests | in progress: dedicated unit suite `tests/test_string_operators` added (21 cases, discovered via CTest); remaining coverage is LPC/JSON round-trip and negative matrix expansion. |

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
- Transparent aliases are a staging step; runtime checks are the effective
  enforcement until abstract handles are enabled.
