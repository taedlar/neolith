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
| `ON` (current) | ✓ contract-bearing shared/malloc boundaries require explicit bridge conversion | ✓ contract violations fail fast in release builds |
| `ON` (target) | ✓ broaden compile-time boundary coverage beyond current contract APIs | ✓ runtime checks remain secondary defense |

### Near-Term Focus

- Keep comparison byte-span coverage stable; production comparison paths are
  complete for current scope (equality/inequality, ordering operators, shared
  helper paths).
- Promote transparent boundary handles to abstract handles so boundary misuse fails at
  compile time.
- Keep JSON boundary contract coverage stable, including CURL ingress
  (`buffer -> from_json`), `perform_using` C-string boundaries, and explicit
  UTF-8/error-handling behavior.
- Maintain regression coverage for JSON/CURL efun boundaries (`perform_using`,
  `c_str()`, `to_json()`) as option surface evolves.

## Status

| Stage | Status |
|---|---|
| Foundation: `blkend` model + shared-table non-NUL lookup + initial safety/tests | complete |
| Implementation: VM/operator NUL-removal and span API normalization | complete |
| Implementation: comparison byte-span readiness | complete |
| Implementation: abstract typed handles + runtime contract enforcement | complete |
| Implementation: C++ RAII wrapper adoption on exception baseline | complete |
| Implementation: efun byte-span readiness | complete for narrowed JSON/CURL ingress scope (broad LPC string-efun hardening remains deferred by plan) |
| Implementation: JSON boundary contract and tests | complete |
| Validation: end-to-end LPC/JSON regression matrix | complete |

## Pre-Closing Handoff

As of 2026-04-18, this plan is being closed for the current scoped milestone.

Non-complete items intentionally deferred beyond this closure:

- Broad LPC string-efun hardening outside the narrowed JSON/CURL ingress scope.
- Future CURL option-surface follow-through, where new text options must preserve
  the explicit `c_str()` / `to_json()` boundary contract and matching regression
  coverage.

## Current State Handoff

As of 2026-04-17 (updated):

### Milestone: `u.string` fully eliminated

A workspace-wide scan of all `*.c`, `*.cpp`, `*.h`, `*.hpp` under `src/` and
`lib/` returns **zero** remaining `u.string` accesses. All reads and writes now
go through typed members (`u.shared_string`, `u.malloc_string`, `u.const_string`)
or through `SVALUE_STRPTR(...)` at subtype-unknown sites.

### What Changed Since Last Update

As of 2026-04-17, the long hardening sweep from prior updates is complete and
the plan now tracks only active follow-on work.

Completed milestones since the previous handoff:

- Typed string-member migration is complete across production sources:
  `u.string` accesses were eliminated in favor of typed members and
  `SVALUE_STRPTR(...)` at subtype-unknown sites.
- Open-coded string subtype stamping was replaced with checked helpers
  (`SET_SVALUE_SHARED_STRING`, `SET_SVALUE_MALLOC_STRING`,
  `SET_SVALUE_CONSTANT_STRING`) across the targeted core/efun areas.
- Boundary/runtime safety hardening is in place (`STRING_TYPE_SAFETY`),
  including typed push/put boundaries and centralized payload classifiers.
- C++ RAII adoption targets in scope are complete (unit-test migration and
  selected production C++ cleanup in JSON paths).
- JSON/CURL boundary contract work is complete for this phase, including
  UTF-8 validation behavior, embedded-NUL preservation, and round-trip/ingress
  regression coverage.
- Equality/inequality operator production semantics are byte-span based and
  covered by direct operator tests (`f_eq`/`f_ne`) in StringOperators suites.
- Test strategy was tightened for this milestone: equality tests now execute
  real operator paths (`f_eq`/`f_ne`) on stack operands instead of simulating
  behavior with helper-level `memcmp` assertions.
- Ordering comparison migration is complete for current scope:
  `f_lt`/`f_le`/`f_gt`/`f_ge` use byte-span lexical comparison semantics
  (embedded `\0` participates; length is tie-breaker after equal prefixes),
  and array helper string comparison paths (`sameval`, builtin sort comparators)
  now route through the same span-aware compare behavior.
- Regression coverage now includes direct ordering-operator tests with embedded
  null bytes (`OrderingOperatorsUseByteSpanForEmbeddedNul`,
  `OrderingOperatorsUseLengthAfterEqualPrefix`) plus array helper equality
  semantics (`SamevalUsesByteSpanForEmbeddedNulStrings`).
- LPC-level ordering validation now includes `sort_array` with embedded-null
  strings (`LpcSortArrayOrdersEmbeddedNulByByteSpan`), with inputs constructed
  via `from_json("\"...\\u0000...\"")` to ensure full byte-span payloads are
  exercised through real efun/operator/runtime paths.
- Directional coverage for LPC sort ordering is now explicit: both ascending
  and descending embedded-null cases are regression-tested
  (`LpcSortArrayOrdersEmbeddedNulByByteSpan`,
  `LpcSortArrayOrdersEmbeddedNulByByteSpanDescending`).
- JSON/CURL ingress hardening advanced at the `perform_using` boundary:
  string values for `url`, `headers`, and string `post_data`/`body` now reject
  embedded NUL bytes, while binary payload intent remains supported via
  `buffer` for `post_data`/`body`.
- C++ wrapper-style read access was expanded in `lib/curl/curl_efuns.cpp`
  for `perform_using`/`perform_to` validation and string-boundary checks via
  `lpc::const_svalue_view`, reducing direct raw `svalue_t` field access on
  those paths.
- `perform_using` embedded-NUL rejection matrix coverage is now complete for
  current option scope: regression tests cover `url`, string `body`,
  string `headers`, and string-array `headers` entry rejection paths, plus the
  binary body `buffer` allow-path.
- JSON-derived boundary routing coverage is now explicit for current CURL
  text-option scope: `perform_using(url, from_json(...))` rejects embedded-NUL
  strings, while `perform_using(url, c_str(from_json(...)))` succeeds via
  explicit C-string boundary conversion; similarly,
  `perform_using(body, to_json(from_json(...)))` succeeds where
  `perform_using(body, from_json(...))` is rejected.

Validation snapshot:

- Linux validation is green (`ctest --preset ut-linux`).
- Cross-platform validation for the counted-string hardening set remains green
  (`vs16-x64`, `vs16-win32`, `clang-x64`).
- As of 2026-04-17, full test runs on `vs16-x64` and `clang-x64` are confirmed
  green for this milestone.
- As of 2026-04-17, post-hardening Linux CTest sweep is green after adding
  `perform_using` header matrix and `c_str()`/`to_json()` boundary-routing
  regressions.

Still intentional / not candidates for replacement:

- The remaining open-coded stamping sites in `src/stack.c` are intentional;
  they are the typed push boundary helpers themselves and are guarded by
  runtime contract checks.

Notable bug fix retained for handoff context:

- Zero-length shared-string payload classification was corrected in
  `src/stralloc.h` (`is_shared_string_payload`) by routing empty spans through
  NUL-terminated lookup. Regression coverage exists in
  `tests/test_stack_machine` (`typedPushSharedStringAcceptsEmptySharedPayload`).

### Next Focus

- **JSON/CURL boundary follow-through** — keep existing JSON/CURL ingress
  coverage stable as counted-string and efun refactors continue, and add
  targeted regression tests only when new boundary behaviors are introduced.
- **Next high-priority efun hardening (JSON/CURL ingress)** — prioritize
  keeping `c_str()` / `to_json()` boundary-path behavior stable as future
  CURL option surface expands, so explicit C-string conversion remains the only
  truncation boundary and is behaviorally locked.

### Baseline and Out of Scope

- Exception migration (`setjmp`/`longjmp` retirement and C++ guard boundaries)
  is completed baseline and not part of this plan's remaining implementation scope.
- `alloc_cstring` remains intentionally outside counted-string semantics.

## Design Constraints (Canonical)

- LPC runtime string semantics are byte-sequence semantics, not JSON text semantics.
- LPC language-level string operators and generic runtime comparison helpers
  should follow byte-span semantics equivalent to `std::string` lexical
  behavior: compare full byte ranges, do not stop at embedded `\0`, and use
  length as a tie-breaker after equal prefixes.
- Efun APIs that are explicitly documented as C-string-oriented keep that
  contract unless intentionally revised. In particular, `strcmp()` remains a
  C-like boundary API and is not the semantic source of truth for LPC operator
  behavior.
- Native-library-facing efuns that ultimately depend on C-string contracts may
  use explicit boundary helpers rather than relying on implicit language
  truncation. `c_str()` is the intended opt-in conversion point when LPC code
  wants to pass a counted string through a C-string semantic boundary such as
  selected CURL text parameters.
- JSON semantics are enforced only at JSON boundaries.
- `from_json` validates UTF-8 at runtime before constructing LPC strings and
  raises an LPC runtime error when input contains invalid UTF-8.
- Identifier-class shared strings remain NUL-terminated by contract.
- This includes function names, variable names, and predefines.
- `u.string` has been removed; string access must use subtype-explicit
  members (`u.const_string`, `u.shared_string`, `u.malloc_string`) or
  `SVALUE_STRPTR(...)` where subtype is not statically known.
- Enforcement mechanism varies by language:
  - **C++ code:** compile-time enforcement via `lpc::svalue_view` / `lpc::svalue` wrappers
  - **C code:** runtime enforcement via `STRING_TYPE_SAFETY` macros + discipline
  - Both paths converge on typed-member-only semantics and explicit subtype intent
- When migrating functions or macros that process `svalue_t`, always validate all runtime string semantics: `STRING_MALLOC`, `STRING_SHARED`, `STRING_CONSTANT`.

## UTF-8 Compatibility Contract

- Counted-string storage is byte-oriented, not Unicode-scalar-oriented.
  Driver-level and LPC-level length semantics count bytes, not UTF-8 characters.
- Counted strings are not globally required to be valid UTF-8.
  Invalid UTF-8 byte sequences may exist in LPC strings unless an operation
  explicitly requires UTF-8 validity.
- **Embedded null bytes (U+0000) are valid UTF-8 and allowed in LPC strings.**
  In JSON, the null character is encoded as `\u0000` and parses to an LPC string
  containing an embedded null byte (0x00). This is permitted because LPC strings
  are byte-sequences, not C strings. `from_json` preserves the full byte-span,
  including embedded nulls. This is a JSON encode/decode data contract and does
  not change generic efun boundary assumptions that still require explicit
  boundary handling where C-string APIs are involved.
- UTF-8 validity is enforced at API boundaries that require text semantics.
  `from_json` rejects invalid UTF-8 (malformed byte sequences like 0xC3 0x28)
  and raises an LPC runtime error before producing an LPC string.
  Other efuns with text/character semantics (for example `explode`) may also
  reject invalid UTF-8 and raise runtime errors for that operation.
- UTF-8 character counting via `explode` is an operation-specific result and
  must not be treated as LPC string length.
  LPC string length and driver counted-string length remain byte counts.

## Compile-Time Safety Scope (C++ Only)

**Decision:** Compile-time string type safety enforcement is scoped to C++ code
only in this plan. The rationale:

1. **C++ code** gets full compile-time safety via `lpc::svalue_view` (non-owning)
   and `lpc::svalue` (owning RAII) wrappers:
   - `set_shared_string()` / `set_malloc_string()` / `set_constant_string()` atomically
     stamp type + subtype + union member (no partial state possible)
   - Typed read accessors (`shared_string()`, `malloc_string()`, `const_string()`)
     communicate ownership intent and preconditions
   - Move/copy semantics + self-assignment guards prevent ownership bugs
   - Unit tests establish conventions for broader future adoption

2. **C code** relies on runtime validation via `STRING_TYPE_SAFETY` macros:
   - `SET_SVALUE_SHARED_STRING()` / `SET_SVALUE_MALLOC_STRING()` / `SET_SVALUE_CONSTANT_STRING()`
     validate payloads at assignment time in debug builds (abort on mismatch)
   - Payload classifiers (`is_shared_string_payload()` / `is_malloc_string_payload()`)
     centralized in `src/stralloc.h` catch sophisticated misuses
   - C code discipline: must use typed helpers, never raw `sv->u.X` access
   - `STRING_TYPE_SAFETY` enabled by default in development (enforced via `cmake/options.cmake`)

Converting all legacy C code to use RAII would be a long-term effort (thousands
of sites). This plan prioritizes establishing solid C++ conventions first via
unit tests, which will then inform eventual C migration guidance. The runtime
checks provide a secondary defense layer for C code until migration is feasible.

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
  static inline shared_str_handle_t to_shared_str(shared_str_t p) {
    shared_str_handle_t h = { p };
    return h;
  }
  static inline malloc_str_handle_t to_malloc_str(malloc_str_t p) {
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
| P1 | C++ wrapper adoption on baseline boundaries | C++ wrappers around `svalue_t` | `lpc::svalue_view`/`lpc::svalue` introduced without C ABI layout change; no duplicate exception-boundary rewrites are introduced; wrapper move/dtor are `noexcept`; unit-test-first ownership migration for counted-string targets is complete (no `free_string_svalue` / `free_svalue` in `tests/**/*.cpp`); remaining work is production C++ efun/helper adoption and any targeted perf checks for newly touched hot paths. |
| P1 | Efun byte-span readiness (deferred broad LPC string paths) | [lib/efuns/string.c](../../lib/efuns/string.c), [lib/efuns/unsorted.c](../../lib/efuns/unsorted.c), [lib/efuns/sprintf.c](../../lib/efuns/sprintf.c), [lib/efuns/sscanf.c](../../lib/efuns/sscanf.c), [lib/curl/curl_efuns.cpp](../../lib/curl/curl_efuns.cpp) | complete for narrowed JSON/CURL ingress scope: `perform_using` rejects embedded-NUL string values for `url`/`headers`/string body, preserves binary payload path via `buffer`, and regression coverage includes header single/array rejection plus explicit `c_str()`/`to_json()` routing checks for JSON-derived string inputs in CURL text options. Broad LPC string-efun hardening remains deferred by plan. |
| P1 | LPC operator semantics vs C-string efuns | [lib/lpc/operator.c](../../lib/lpc/operator.c), [lib/lpc/array.c](../../lib/lpc/array.c), [docs/efuns/strcmp.md](../../docs/efuns/strcmp.md) | complete for current scope: equality/inequality and grouping/ordering helper paths use full byte-span behavior (including embedded null participation); `strcmp()` remains explicitly documented and tested as C-string-oriented boundary behavior. |
| P1 | JSON boundary contract (priority: CURL buffer ingress) | JSON efuns/helpers (`from_json`, `to_json`) and CURL ingress paths | complete: contract docs state LPC byte spans vs JSON text; `from_json` rejects invalid UTF-8 and raises LPC runtime error on invalid sequences; coverage includes explicit UTF-8 pass/fail tests (`fromJsonValidUtf8StringAccepted`, `fromJsonInvalidUtf8StringError`, `fromJsonInvalidUtf8BufferError`) plus buffer ingress success/error/size paths (`fromJsonBuffer`, `fromJsonInvalidBufferError`, `fromJsonLargeBuffer`) and a CURL callback payload integration test (`CurlBufferPayloadParsesViaFromJson`); JSON strings containing embedded null bytes are stored and copied byte-for-byte (full span), not truncated at C-string boundaries. |
| P1 | Unicode and escape consistency | JSON encode/decode implementation | complete: encoder/decoder symmetry is now covered for control escapes, `\\`, `\"`, `\uXXXX`, surrogate pairs, and non-BMP round-trips via `toJsonControlEscapes`, `fromJsonControlEscapes`, `fromJsonSurrogatePairAccepted`, `fromJsonLoneHighSurrogateError`, and `roundTripNonBmpCharacter`; contract docs are aligned in `docs/efuns/from_json.md` and `docs/efuns/to_json.md`. |
| P2 | End-to-end regression matrix | LPC-level and efun/unit tests | complete for current hardening scope: dedicated unit suite `tests/test_string_operators` added (21 cases, discovered via CTest), and full matrix validation is passing on Linux, VS16 x64, VS16 win32, and clang x64. Future LPC/JSON round-trip and negative-matrix additions remain follow-on expansion work. |

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
- Transparent aliases are a staging step; runtime checks block misuse until
  abstract handles make explicit intent mandatory at compile time.
- Equality/inequality coverage is most reliable when tests execute `f_eq`/
  `f_ne` on real stack operands rather than simulating behavior with direct
  `memcmp` checks; this catches operator stack/result semantics and cleanup
  behavior that helper-level assertions can miss.
