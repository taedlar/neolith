# Counted String Length Representation

Consolidates the length-storage change to `malloc_block_t`, its invariants, and a
catalogue of paths that still depend on NUL termination. < 300 words.

## Summary

The `unused` pointer field in `malloc_block_t` was renamed to `blkend` and given
meaning: when `size == USHRT_MAX` (the sentinel value for strings longer than 65534
bytes), `blkend` points one byte past the end of the string payload. This makes
`COUNTED_STRLEN` O(1) for all counted strings and removes the last remaining
`strlen` scan inside the counted-string fast path.

A complementary invariant is enforced: **STRING_SHARED strings are capped at
USHRT_MAX − 1 bytes** at allocation time (`alloc_new_string`, `make_shared_string`).
This keeps the `size == USHRT_MAX` sentinel exclusive to STRING_MALLOC, so generic
counted-string macros do not need to branch on subtype to decide whether `blkend` is
valid.

## Status

| Stage | Status |
|---|---|
| Rename `unused` → `blkend`, wire on all malloc create/resize/unlink paths | complete |
| `COUNTED_STRLEN` O(1) long-string path via `blkend` | complete |
| Shared-string length cap below USHRT_MAX | complete |
| Unit tests for all blkend paths | complete |
| `STRING_TYPE_SAFETY` cmake option: typed aliases + always-on runtime guards | complete |

## Rationale

Before this change the only way to recover the length of a counted string longer than
65534 bytes was `strlen(str + USHRT_MAX) + USHRT_MAX` — a full scan past the first
65535 bytes every time. The `blkend` field turns that into a two-pointer subtraction
kept consistent by every allocation and resize path.

## Header-backed `char *` contracts and type safety

A raw `char *` carries no runtime tag indicating whether it points to a
`STRING_SHARED` or `STRING_MALLOC` payload. The decision is made from one of two
sources:

1. `svalue_t.subtype` (`STRING_MALLOC`, `STRING_SHARED`, `STRING_COUNTED`,
   `STRING_HASHED`) for generic runtime paths that branch before touching a
   block header.
2. Function contract and pointer provenance for the small set of public
   functions that take a typed bare `char *`.

### Contract-bearing functions

| Function | Expected pointer kind | Access pattern |
|---|---|---|
| `ref_string(shared_str_t)` | `STRING_SHARED` payload | Uses `BLOCK(str)`, verifies via `findblock` |
| `free_string(shared_str_t)` | `STRING_SHARED` payload | Uses `BLOCK(str)`, removes from shared table |
| `extend_string(malloc_str_t, size_t)` | `STRING_MALLOC` payload | Reallocates from `MSTR_BLOCK(str)` |
| `push_shared_string(char *)` | `STRING_SHARED` payload | Delegates to `ref_string()` |
| `push_malloced_string(char *)` | `STRING_MALLOC` payload | Sets svalue subtype to `STRING_MALLOC` |

Constructor and plain C-string functions (`make_shared_string`, `findstring`,
`int_new_string`, `int_string_copy`, `int_alloc_cstring`) accept ordinary
C strings or return typed pointers by construction and are not part of this
contract.

### Type-safety hardening (`STRING_TYPE_SAFETY`)

The `STRING_TYPE_SAFETY` cmake option (default: ON) adds two layers of hardening.

**Layer 1 — typed aliases.**  `stralloc.h` exports `shared_str_t` and
`malloc_str_t` as transparent `typedef char *` aliases.  Function signatures
for the contract-bearing functions use these types so intent is visible at
every declaration site.  Because the typedefs are transparent in all build
modes, no existing call sites require changes and there is no runtime overhead.

**Layer 2 — always-on runtime guards.**  Under `STRING_TYPE_SAFETY`, `extend_string`
and `int_string_unlink` check their pointer contract even in NDEBUG/release
builds — not only via debug-mode `assert`.  A ref count of 0 on entry to either
function (indicating an immortal `STRING_SHARED` or freed pointer) triggers
`debug_fatal` + `abort`.

### Path to full compile-time enforcement

Changing `shared_str_t` and `malloc_str_t` from transparent aliases to opaque
struct pointer types would turn contract violations into compile errors.  This
requires:

1. Updating struct fields and variables that store shared/malloc strings from
   `char *` to the appropriate typedef.
2. Adding explicit cast macros (`SHARED_STR(x)`, `MALLOC_STR(x)`) at call
   sites where the type must be converted.

This change is deferred because it affects a large portion of the codebase;
the typed function signatures and always-on runtime guards provide sufficient
protection for now.

## Paths still relying on NUL termination

These are **not addressed** by this change. Any future move to binary-safe (embedded
NUL) strings must fix each one.

### Shared-string table
| Location | Reason |
|---|---|
| [src/stralloc.c `sfindblock`](../../src/stralloc.c) | Uses `strcmp` and first-char comparison for hash-chain lookup. |
| [src/stralloc.c `alloc_new_string`](../../src/stralloc.c) | Measures input with `strlen` before storing. |
| [src/stralloc.c `make_shared_string`](../../src/stralloc.c) | Measures input with `strlen` to build the truncated key. |

### Malloc string creation and copy
| Location | Reason |
|---|---|
| [src/stralloc.c `int_string_copy`](../../src/stralloc.c) | Measures source with `strlen`. |
| [src/stralloc.c `int_alloc_cstring`](../../src/stralloc.c) | Plain `strdup`-equivalent; no block header. |
| [src/stralloc.c `unlink_string_svalue` STRING_SHARED branch](../../src/stralloc.c) | Copies via `strncpy` using `SHARED_STRLEN` (safe for counted), but the source shared string is still NUL-terminated by construction. |

### VM string operations
| Location | Reason |
|---|---|
| [src/interpret.h `EXTEND_SVALUE_STRING`](../../src/interpret.h) | Appended C string `y` measured with `strlen`. |
| [src/interpret.h `SVALUE_STRING_ADD_LEFT`](../../src/interpret.h) | Prefix `y` measured with `strlen`. |
| [src/interpret.h `SVALUE_STRING_JOIN`](../../src/interpret.h) | Second operand copied with `strcpy`. |
| [src/stralloc.h `SVALUE_STRLEN`](../../src/stralloc.h) | Falls back to `strlen` for STRING_CONSTANT. |

### String efuns
| Location | Reason |
|---|---|
| [lib/efuns/string.c `f_strcmp`](../../lib/efuns/string.c) | Uses `strcmp`. |
| [lib/efuns/string.c case-conversion loops](../../lib/efuns/string.c) | Iterates to NUL. |
| [lib/efuns/unsorted.c member-search](../../lib/efuns/unsorted.c) | Uses `strcmp` for string array lookup. |
| [lib/efuns/replace_program.c](../../lib/efuns/replace_program.c) | Uses `strlen` on the program name svalue. |
| [lib/efuns/sprintf.c](../../lib/efuns/sprintf.c) | Iterates `*str` loop to NUL for `%s` column filling. |
| [lib/efuns/sscanf.c](../../lib/efuns/sscanf.c) | Treats format and input strings as C strings. |

### Driver internals
| Location | Reason |
|---|---|
| [src/apply.c function name lookup](../../src/apply.c) | `strcmp` on function names (shared strings, bounded by identifier length). |
| [src/comm.c / output paths](../../src/comm.c) | Sends `u.string` directly to network write; relies on NUL or explicit length from `SVALUE_STRLEN`. |
| [lib/lpc/mapping.c restore path](../../lib/lpc/mapping.c) | Parses saved-object text; string values reconstructed from NUL-terminated C buffers. |

## Lessons Learned

- Enforcing the size-cap invariant only in the _allocator_ (`alloc_new_string` /
  `make_shared_string`) is sufficient: callers never see a shared string at the
  sentinel length.
- Canonicalizing the truncated key _before_ the hash lookup in `make_shared_string`
  ensures oversized inputs deduplicate to the same shared block rather than creating
  a second block that `findstring` can never locate by the original key.
- The fallback `strlen` path in `COUNTED_STRLEN` (for `blkend == NULL`) is needed
  for compatibility with older binary caches loaded before the change; it can be
  removed once binary format is bumped.
- Transparent `typedef char *` aliases enforce nothing at compile time; they serve
  as documentation only until call sites are migrated to opaque struct pointer types.
  The runtime guards (Layer 2) are therefore the primary enforcement mechanism
  while the codebase is being incrementally migrated.
