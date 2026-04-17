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

- Promote transparent boundary handles to abstract handles so boundary misuse fails at
  compile time.
- Keep JSON boundary contract coverage stable, including CURL ingress
  (`buffer -> from_json`) and explicit UTF-8/error-handling behavior.
- Expand regression coverage to lock in counted-string semantics across LPC,
  efuns, and JSON boundaries.

## Status

| Stage | Status |
|---|---|
| Foundation: `blkend` model + shared-table non-NUL lookup + initial safety/tests | complete |
| Implementation: VM/operator NUL-removal and span API normalization | complete |
| Implementation: abstract typed handles + runtime contract enforcement | complete |
| Implementation: C++ RAII wrapper adoption on exception baseline | complete |
| Implementation: efun byte-span readiness | in progress (narrowed: defer broad LPC string-efun hardening; prioritize JSON/CURL ingress) |
| Implementation: JSON boundary contract and tests | complete |
| Validation: end-to-end LPC/JSON regression matrix | complete |

## Current State Handoff

As of 2026-04-17 (updated):

### Milestone: `u.string` fully eliminated

A workspace-wide scan of all `*.c`, `*.cpp`, `*.h`, `*.hpp` under `src/` and
`lib/` returns **zero** remaining `u.string` accesses. All reads and writes now
go through typed members (`u.shared_string`, `u.malloc_string`, `u.const_string`)
or through `SVALUE_STRPTR(...)` at subtype-unknown sites.

### What Changed Since Last Update

Four additional hardening batches landed after the previous handoff:

**Batch A — efun array constructors and core runtime sites:**
- `lib/efuns/variable.c` — shared-string array item constructors → `SET_SVALUE_SHARED_STRING`
- `lib/efuns/call_out.cpp` — callout function-name item constructors → `SET_SVALUE_SHARED_STRING`
- `lib/efuns/file_utils.c` — `encode_stat` array/scalar and `check_valid_path` → `SET_SVALUE_MALLOC_STRING`
- `lib/efuns/debug.c` — `f_throw`, `f_call_stack` (all 4 cases), `fms_recurse`, `f_memory_summary` → checked helpers

**Batch B — curl and socket libs:**
- `lib/curl/curl_efuns.cpp` — all 5 `u.string` reads in `T_STRING`-guarded paths → `SVALUE_STRPTR(...)`
- `lib/socket/socket_efuns.c` — 1 write-side constructor → `SET_SVALUE_SHARED_STRING`; 6 read-side callback and message-send paths → `SVALUE_STRPTR(...)`

**Batch C — operator, rc, efun fallbacks and function-info builders:**
- `lib/lpc/operator.c` — `f_range` and `f_extract_range` empty-string returns → `SET_SVALUE_CONSTANT_STRING`
- `lib/rc/rc.cpp` — `get_config_item` string fallback → `SET_SVALUE_CONSTANT_STRING`
- `lib/efuns/command.c` — `f_query_notify_fail` shared push → `SET_SVALUE_SHARED_STRING`
- `lib/efuns/json.cpp` — mapping key construction → `SET_SVALUE_CONSTANT_STRING`
- `lib/efuns/string.c` — `f_repeat_string` zero-repeat fallback → `SET_SVALUE_CONSTANT_STRING`
- `lib/efuns/unsorted.c` — `f_query_privs` out-of-order stamp and all shared stamps in `f_functions` → typed helpers

**Batch D — remaining open-coded stamping sweep:**
- `lib/efuns/parse.c` — all 6 remaining multi-branch stamps → `SET_SVALUE_SHARED_STRING` / `SET_SVALUE_MALLOC_STRING` / `SET_SVALUE_CONSTANT_STRING`
- `lib/efuns/sprintf.c` — all 4 remaining `clean`/return-path stamps → typed helpers
- `lib/efuns/sscanf.c` — string assignment macro now routes through `SET_SVALUE_MALLOC_STRING`
- `lib/efuns/unsorted.c` — `f_function_exists` result stamp now uses `SET_SVALUE_MALLOC_STRING` (also fixes missing explicit `type = T_STRING`)
- `lib/lpc/array.c` — all 18 remaining explode/implode/comparator/reg_assoc stamps → typed helpers
- `lib/lpc/mapping.c` — all remaining restore/insert/add_* mapping string stamps → typed helpers
- `lib/lpc/object.c` — all remaining restore string stamps → typed helpers

All targeted regression tests remain green after each batch.
Full Linux matrix validation (`ctest --preset ut-linux`) is now passing after
the remaining open-coded stamping sweep.
Full cross-platform validation is now also passing on `vs16-x64`,
`vs16-win32`, and `clang-x64`.
Post-enforcement validation is also green after removing `svalue_u.string`:
`ctest --preset ut-linux` continues to pass.

### Remaining Open-Coded Stamping Sites (5 total)

| File | Count | Notes |
|---|---|---|
| `src/stack.c` | 5 | **Intentional** — these are the typed push boundary helpers themselves |

The `src/stack.c` sites are not candidates for replacement; they implement the
checked boundary with runtime domain checks under `STRING_TYPE_SAFETY`.

### Bug Fix: Zero-Length Shared-String Classifier

The `STRING_TYPE_SAFETY` payload classifier `is_shared_string_payload()` in
`src/stralloc.h` incorrectly rejected zero-length shared strings because
`findstring(p, p + 0)` returns `NULL` for empty spans. Fixed by routing empty
payloads through `findstring(p, NULL)` (NUL-terminated lookup). This closed a
real runtime abort in `SimulEfunsTest.callSimulEfun` caused by LPC code
assigning `result = ""`. A regression test was added in
`tests/test_stack_machine` (`typedPushSharedStringAcceptsEmptySharedPayload`).

- Read-side intent tightening has started: subtype-known `STRING_MALLOC` fast
  paths in `src/interpret.h` now read `u.malloc_string`, and subtype-specific
  memory accounting in `lib/efuns/debug.c` now reads
  `u.malloc_string`/`u.shared_string` directly. Current remaining
  `u.string` counted-ref uses were in generic `STRING_COUNTED` helper-style
  code paths (`lib/lpc/svalue.c`, `lib/lpc/array.c`). These are now tightened
  to explicit `STRING_MALLOC`/`STRING_SHARED` read branches.
- Core helper macros now also prefer typed reads: `SVALUE_STRLEN`/
  `SVALUE_STRLEN_DIFFERS` in `src/stralloc.h` route counted strings through
  typed members, and interpreter string composition macros in `src/interpret.h`
  use `SVALUE_STRPTR(...)` for source reads.
- Remaining subtype-known shared free path in `lib/lpc/operator.c` switch
  handling now uses `u.shared_string` at the boundary conversion call site
  (`free_string(to_shared_str(...))`), eliminating the last `to_shared_str`
  call that consumed `u.string` directly.
- `src/interpret.h` string append/join/prepend macros now read source payloads
  through typed helpers (`SVALUE_STRPTR`), and the remaining direct
  `u.string` source read in the append slow path has been removed.
- Additional read-side tightening completed in `lib/efuns/unsorted.c`
  (`member_array` string path and counted-string comparison fast-path) and
  `src/stralloc.c` (`free_string_svalue` now selects typed payload members
  directly for counted strings).
- `src/interpret.c` string reads now consistently use typed string-pointer
  helpers in callback dispatch setup, string range assignment paths,
  string comparison in loop conditions, foreach string iterator setup,
  string index/rindex operations, and string-return trace logging.
- Additional `src` read-side cleanup landed in `src/error_context.cpp`,
  `src/command.c`, and selected type-guarded `T_STRING` paths in
  `src/simulate.c` (master-uid retrieval flow, add_action string branch,
  scoped object lookup for `do_message`, first_inventory string lookup,
  tell_room/print_svalue string branches, and input/get_char callback
  function-name dispatch logging and lookup).
- `src` now has no remaining `u.string` occurrences in C/C++ sources, and
  bridge helper signatures in `src/stralloc.h` are tightened to typed payload
  aliases (`to_shared_str(shared_str_t)`, `to_malloc_str(malloc_str_t)`) to
  make boundary intent explicit at call sites.
- Resolver cache bookkeeping in `src/addr_resolver.cpp` now marks shared
  string ownership explicitly (`reverse_cache_entry_t::name` and
  `forward_cache_entry_t::hostname` use `shared_str_t`), keeping bridge
  free paths (`free_string(to_shared_str(...))`) aligned with typed intent.
- `lib/lpc/types.h` C++ view accessor `svalue_view::c_str()` now routes
  through subtype-safe `SVALUE_STRPTR(...)` with a string-type guard rather
  than reading `u.string` directly.
- Additional `lib/lpc/operator.c` string-typed comparator/range/switch paths
  now read via `SVALUE_STRPTR(...)` (equality/inequality memcmp branches,
  relational string compares, range/extract source pointers, and non-shared
  switch-label lookup/logging).
- `lib/lpc/array.c` string-typed helper/comparator/callback and regexp paths
  now consistently read via `SVALUE_STRPTR(...)` (implode/sameval,
  `unique_array`/`objects` callback-name reads, `map_string` and object lookup,
  sort/alist/intersect/subtract shared-string normalization, `match_regexp`,
  and `reg_assoc` pattern compilation).
- `lib/lpc/object.c` save-serialization string reads (`svalue_save_size`,
  `save_svalue`) and `lib/lpc/mapping.c` string-key normalization in
  `svalue_to_int()` now use `SVALUE_STRPTR(...)` in string-typed paths.
- Initial `lib/efuns` read-side tightening is now in place for
  `inventory.c` and `uids.c` (string arguments in `f_move_object()` and
  `f_seteuid()` now route through `SVALUE_STRPTR(...)`).
- Additional low-risk `lib/efuns` single-site string reads are now tightened
  in `dump_prog.c` (`f_dump_prog` optional output path) and `maps.c`
  (`f_match_path` source-path walker) via `SVALUE_STRPTR(...)`.
- `lib/efuns/datetime.c` timezone argument reads in
  `f_is_daylight_savings_time()` and `f_zonetime()` now use
  `SVALUE_STRPTR(...)`.
- `lib/efuns/dumpstat.c` now uses `SVALUE_STRPTR(...)` in string-size
  accounting (`svalue_size`) and `f_dumpallobj()` argument forwarding.
- `lib/efuns/sscanf.c` and `lib/efuns/ed.c` now route string-typed parse,
  format, and returned-path reads through `SVALUE_STRPTR(...)`.
- `lib/efuns/json.cpp` now uses `SVALUE_STRPTR(...)` for LPC string input,
  mapping-key emission, and JSON string serialization paths.
- `lib/efuns/interactive.c` and `lib/efuns/call_out.cpp` now route
  string-typed message, resolver, and callout-name arguments through
  `SVALUE_STRPTR(...)`.
- `lib/efuns/replace_program.c` now uses `SVALUE_STRPTR(...)` for
  replacement-program name handling and ignore-list string lookup.
- `lib/efuns/call_other.c`, `lib/efuns/debug.c`, and
  `lib/efuns/tell_object.c` now route string-typed object/function/message
  arguments through `SVALUE_STRPTR(...)`.
- `lib/efuns/sockets.c` now uses `SVALUE_STRPTR(...)` for string address
  parsing and optional address forwarding in `socket_connect()` and
  `socket_write()`.
- `lib/efuns/variable.c` now uses `SVALUE_STRPTR(...)` for variable-name
  lookup/error reporting and restore-variable input handling.
- `lib/efuns/bits.c` now uses `SVALUE_STRPTR(...)` for bitfield string reads
  and `u.malloc_string` after unlinking mutable bitfield strings.
- `lib/efuns/command.c` now routes living-name lookup, add/remove action,
  command dispatch, and notify-fail string arguments through
  `SVALUE_STRPTR(...)`.
- `lib/efuns/sprintf.c` now uses `SVALUE_STRPTR(...)` across string
  serialization, column/table formatting, justification, and top-level
  sprintf/printf format-string entry points.
- `lib/efuns/unsorted.c` now routes string-typed paths through
  `SVALUE_STRPTR(...)` for child/clone/object lookup, `ed*` entry points,
  function existence lookup, privilege assignment, and string-to-number
  conversion helpers.
- `lib/efuns/file.c` now uses `SVALUE_STRPTR(...)` across file path
  forwarding, directory/stat helpers, file byte/string I/O, and
  save/restore object path arguments.
- `lib/efuns/string.c` now uses `SVALUE_STRPTR(...)` across case conversion,
  crypt/hash helpers, implode/explode/regexp helpers, replace-string logic,
  trace logging, and string search/compare paths; mutable paths use
  `u.malloc_string` after unlinking.
- `lib/efuns/parse.c` now routes parse pattern, command word, id-list, and
  adjective/preposition string reads through `SVALUE_STRPTR(...)`, including
  parse-command entry, pluralization, numeral parsing, and object matching.
- Additional low-risk typed-intent tightening is in place around lookup-only
  helpers and typed-return APIs: `find_function_by_name2()` now keeps raw
  C-string lookup separate from `shared_str_t` results, `find_string_in_mapping()`
  and `find_global_variable()` now take `const char *`, and `f_to_json()` keeps
  `string_copy()` results in `malloc_str_t` until the push boundary.
- Test coverage is also tightened to preserve `shared_str_t` / `malloc_str_t`
  through shared-string, malloc-string, and socket address setup paths rather
  than immediately erasing typed returns back to plain `char *` in older tests.
- The dedicated type-safety regression test now expresses bridge-helper return
  expectations in terms of `shared_str_t` / `malloc_str_t` aliases rather than
  raw `char *`, keeping the compile-time contract wording aligned with the
  intended typed payload model.
- The same regression test now also freezes typed signatures and/or result
  types for shared-string lookup/allocation, malloc-string allocation/copy,
  typed push helpers, and malloc-string resize/unlink macro wrappers.
- `src/stack.c` now applies `STRING_TYPE_SAFETY` runtime contract checks at the
  typed push boundaries as well: `push_shared_string()` rejects non-shared
  payloads and `push_malloced_string()` rejects shared payloads before they are
  stamped onto the eval stack. `tests/test_stack_machine` now covers the normal
  shared-refcount and malloc-ownership behavior of these helper APIs.
- Payload-domain checks are now centralized in `src/stralloc.h`
  (`is_shared_string_payload()` / `is_malloc_string_payload()`) and reused by
  both the stack push helpers and `lpc::svalue_view` typed string setters in
  `lib/lpc/types.h`, keeping the C and C++ subtype-stamping boundaries aligned.
- `src/interpret.h` return-value macros `put_shared_string()` and
  `put_malloced_string()` now reuse the same centralized payload checks under
  `STRING_TYPE_SAFETY`, and `tests/test_stack_machine` covers their normal
  shared/malloc subtype stamping behavior.
- Zero-length shared-string payloads are now handled correctly by the
  centralized payload classifiers in `src/stralloc.h`; this closes a real
  runtime bug exposed by `SimulEfunsTest.callSimulEfun`, where LPC code using
  `""` hit the shared-string boundary check even though the payload was valid.
- `lib/lpc/types.h` now provides checked `SET_SVALUE_SHARED_STRING()` /
  `SET_SVALUE_MALLOC_STRING()` / `SET_SVALUE_CONSTANT_STRING()` helpers for
  direct `svalue_t` stamping, and low-risk core sites in `src/outbuf.c`,
  `src/error_context.cpp`, `src/stralloc.c`, and `lib/efuns/json.cpp` now use
  them instead of open-coding `type/subtype/u.*` assignments.
- Initial efun adoption of the checked svalue-stamping helpers is now in place
  for straightforward return/value-construction paths in `file.c`, `string.c`,
  `bits.c`, `unsorted.c`, `datetime.c`, `variable.c`, `call_out.cpp`,
  `file_utils.c`, and selected low-risk `debug.c` constructors.
- Focused regression coverage remains green after this batch, including
  `EfunsTest.throwError`, `EfunsTest.throwWithoutCatchRaisesRuntimeError`,
  `LPCInterpreterTest.throwZeroNormalizesToUnspecifiedError`, and
  `SimulEfunsTest.callSimulEfun`.
- C++ RAII wrapper adoption has started in production code: `lib/efuns/json.cpp`
  now uses `lpc::svalue` for temporary mapping-key construction in
  `json_to_lpc()`, removing a manual `free_string(to_shared_str(...))`
  ownership-release path while preserving the existing C ABI boundary.
- `tests/test_stralloc` now freezes `lpc::svalue` copy and move semantics for
  shared-string refcount retention and malloc-string ownership transfer,
  including copy-assignment, move-assignment, and self-assignment no-op
  coverage.
- Unit-test-first C++ adoption is now underway: `tests/test_lpc_interpreter`
  return-value paths and socket helper `QueryObjectNumberMethod()` sites in
  `tests/test_socket_efuns` now use `lpc::svalue` ownership plus
  `lpc::svalue_view` accessors instead of raw `svalue_t` temporaries with
  manual `free_svalue()` / `free_string_svalue()` cleanup.
- Additional helper-return test adoption is now in place for
  `tests/test_string_operators/test_string_operators_lpc.cpp`
  (`call_noarg() -> lpc::svalue`) and
  `tests/test_lpc_interpreter/test_input_to_get_char.cpp`
  (`make_function_name_svalue() -> lpc::svalue`), reducing repeated raw
  callback-name/return-value construction in C++ test code.
- Counted-string operator tests now adopt RAII ownership end-to-end in
  `tests/test_string_operators/test_string_operators_main.cpp`: local
  owner variables are `lpc::svalue`, macro call sites pass `.raw()`, and
  manual `free_string_svalue()` cleanup in these tests has been removed in
  favor of destructor-based cleanup.
- Socket behavior matrix tests now have an additional RAII adoption slice in
  `tests/test_socket_efuns/test_socket_efuns_behavior.cpp` for
  `SOCK_BHV_001` through `SOCK_BHV_010`: callback locals now use
  `lpc::svalue` ownership and pass `raw()` into C APIs, eliminating manual
  callback `free_string_svalue()`/`free_string(...)` cleanup in this range.
- Focused validation for the migrated socket behavior slice is green:
  all ten tests (`SOCK_BHV_001` … `SOCK_BHV_010`) pass after RAII conversion.
- Socket behavior RAII adoption is now extended in the same file for
  `SOCK_BHV_011` through `SOCK_BHV_020`: callback and payload locals in this
  range are now `lpc::svalue` owners passed via `raw()` to C APIs, with
  manual callback/payload `free_string_svalue()` cleanup removed.
- Focused validation for this second socket slice is also green:
  all ten tests (`SOCK_BHV_011` … `SOCK_BHV_020`) pass after RAII conversion.
- Resolver forward tests now have an initial RAII adoption slice in
  `tests/test_socket_efuns/test_socket_efuns_resolver.cpp` for
  `RESOLVER_FWD_001` through `RESOLVER_FWD_005`: callback locals now use
  `lpc::svalue` ownership and pass `raw()` into C APIs, removing manual
  callback `free_string_svalue()` cleanup in this range.
- Focused validation for this resolver slice is green:
  all five tests (`RESOLVER_FWD_001` … `RESOLVER_FWD_005`) pass after RAII conversion.
- The remaining resolver callback-owner/manual-cleanup site in
  `RESOLVER_CACHE_001_ForwardCacheHit_BypassesDNSWorker` is now also
  converted to `lpc::svalue` ownership (`raw()` passed to C APIs), removing
  the last `free_string_svalue()` callback cleanup pattern in
  `tests/test_socket_efuns/test_socket_efuns_resolver.cpp`.
- Focused validation for `RESOLVER_CACHE_001_ForwardCacheHit_BypassesDNSWorker`
  is green after conversion.
- Socket extensions tests now have a new contiguous RAII adoption slice in
  `tests/test_socket_efuns/test_socket_efuns_extensions.cpp` for
  `SOCK_OP_001` through `SOCK_OP_003` and `SOCK_DNS_001` through
  `SOCK_DNS_005`: callback locals are now `lpc::svalue` owners passed via
  `raw()` to C APIs, with manual callback `free_string_svalue()` cleanup
  removed in this block.
- Focused validation for the extensions slice is green:
  all eight migrated tests (`SOCK_OP_001` … `SOCK_OP_003`,
  `SOCK_DNS_001` … `SOCK_DNS_005`) pass after RAII conversion.
- Socket extensions DNS RAII adoption is now extended in the same file for
  `SOCK_DNS_006`, `SOCK_DNS_011`, `SOCK_DNS_012`, and `SOCK_DNS_013`
  (including the remaining raw-owner path in `SOCK_DNS_005`): callback locals
  now use `lpc::svalue` owners passed via `raw()` into C APIs, with manual
  callback `free_string_svalue()` cleanup removed for this migrated set.
- Focused validation for this second extensions DNS slice is green:
  `SOCK_DNS_005`, `SOCK_DNS_006`, `SOCK_DNS_011`, `SOCK_DNS_012`, and
  `SOCK_DNS_013` all pass after RAII conversion.
- Socket extensions runtime tests now have a final RAII cleanup slice for
  `SOCK_RT_001` through `SOCK_RT_003`: callback locals use `lpc::svalue`
  ownership and pass `raw()` into C APIs, removing remaining manual callback
  `free_string_svalue()` cleanup in this runtime block.
- Focused validation for `SOCK_RT_001` … `SOCK_RT_003` is green after conversion.
- Unit-test-first RAII ownership migration milestone is now complete for the
  current counted-string target set: a workspace scan of `tests/**/*.cpp`
  shows no remaining `free_string_svalue(...)` / `free_svalue(...)` manual
  cleanup paths in C++ unit tests.
- **Production RAII adoption complete**: `f_from_json` in `lib/efuns/json.cpp`
  now uses `std::unique_ptr<boost::json::value>` (via `std::make_unique`) for
  the `parsed` heap slot, eliminating the manual `new`/`delete` pair that
  would have leaked the `boost::json::value` if `json_to_lpc()` triggered an
  LPC runtime error unwind. All 34 JSON-related tests remain green after this
  change. A workspace-wide scan of production C++ confirms no further local
  temporary `svalue_t` or heap objects with manual cleanup remain avoidable
  without changing C ABI boundaries: the remaining `free_svalue(sp, ...)` calls
  in efun bodies are stack-pop operations (eval-stack ABI contract), and
  `apply_ret_value` / `catch_value` are C-linkage globals managed by their
  respective owners.
- JSON UTF-8 boundary test coverage is now explicitly aligned with the
  contract decisions in `tests/test_efuns/test_json.cpp`:
  `fromJsonValidUtf8StringAccepted` (valid UTF-8 accepted),
  `fromJsonInvalidUtf8StringError` (invalid UTF-8 string rejected), and
  `fromJsonInvalidUtf8BufferError` (invalid UTF-8 buffer rejected), in
  addition to existing buffer-path coverage (`fromJsonBuffer`,
  `fromJsonInvalidBufferError`, `fromJsonLargeBuffer`).
- CURL ingress integration coverage now also validates the concrete
  `buffer -> from_json` handoff from a real callback payload in
  `tests/test_efuns/test_curl.cpp` via
  `CurlBufferPayloadParsesViaFromJson`.
- JSON embedded null-byte handling was corrected: `from_json` now preserves
  the full byte-span when parsing JSON strings containing the null character
  (encoded as `\u0000`). The fix changed `json_to_lpc()` to use span-based
  `int_string_copy(data, data+size)` instead of C-string semantics
  `string_copy(c_str())`, ensuring embedded nulls (U+0000) are not truncated.
  Added test `fromJsonEmbeddedNullCharacterAccepted` confirms this behavior.
- JSON round-trip symmetry: `to_json` was also corrected to preserve embedded
  null bytes during serialization. The fix changed `lpc_to_json()` to pass the
  full byte-span to `boost::json::string()` via `string_view(data, size)` instead
  of C-string semantics `boost::json::string(c_str())`. This ensures embedded
  nulls are correctly escaped as `\u0000` in the JSON output. The test
  `roundTripEmbeddedNull` validates the full cycle: LPC string with embedded
  null → to_json (with escaped null) → from_json (reconstructs original).
- JSON object-key embedded-null handling is now covered end to end: decode
  (`fromJsonEmbeddedNullObjectKeyAccepted`) and encode
  (`toJsonEmbeddedNullObjectKeyEscaped`) both preserve full key byte spans.
  The encode path required a mapping canonicalization fix in
  `lib/lpc/mapping.c` (`svalue_to_int()` now interns string keys with
  `make_shared_string(start, end)` instead of NUL-terminated lookup).
- Unicode/escape contract coverage is expanded and passing in
  `tests/test_efuns/test_json.cpp`: control escapes
  (`toJsonControlEscapes`, `fromJsonControlEscapes`), surrogate handling
  (`fromJsonSurrogatePairAccepted`, `fromJsonLoneHighSurrogateError`), and
  non-BMP round-trip behavior (`roundTripNonBmpCharacter`).
- JSON contract docs are now aligned with implementation and tests in
  `docs/efuns/from_json.md` and `docs/efuns/to_json.md`, including explicit
  embedded-null, escape, and surrogate/non-BMP statements.

### Next Focus

- **JSON/CURL boundary follow-through** — keep existing JSON/CURL ingress
  coverage stable as counted-string and efun refactors continue, and add
  targeted regression tests only when new boundary behaviors are introduced.
- **Operator semantic migration** — migrate LPC string comparison/grouping/
  ordering paths from C-string semantics to byte-span semantics while keeping
  explicitly C-like efuns such as `strcmp()` unchanged.

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
| P1 | Efun byte-span readiness (deferred broad LPC string paths) | [lib/efuns/string.c](../../lib/efuns/string.c), [lib/efuns/unsorted.c](../../lib/efuns/unsorted.c), [lib/efuns/sprintf.c](../../lib/efuns/sprintf.c), [lib/efuns/sscanf.c](../../lib/efuns/sscanf.c) | Scope is intentionally narrowed for this phase: no broad LPC-side behavioral expansion unless required by JSON/CURL boundary safety. Any touched path must preserve LPC compatibility and existing tests remain green. |
| P1 | LPC operator semantics vs C-string efuns | [lib/lpc/operator.c](../../lib/lpc/operator.c), [lib/lpc/array.c](../../lib/lpc/array.c), [docs/efuns/strcmp.md](../../docs/efuns/strcmp.md) | LPC operators and generic comparison helpers use full byte-span ordering/equality semantics compatible with `std::string`; embedded null bytes participate in compare/sort/group operations; `strcmp()` remains explicitly documented and tested as C-string-oriented behavior. |
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
- `SVALUE_STRLEN_DIFFERS` using only `MSTR_SIZE` is conservative for sentinel
  long malloc strings (`size == USHRT_MAX`); exact long-string mismatch
  fast-paths should consult counted logical length (`COUNTED_STRLEN`).
- Transparent aliases are a staging step; runtime checks block misuse until
  abstract handles make explicit intent mandatory at compile time.
