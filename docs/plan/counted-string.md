# Counted and Shared String Length Representation

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

Shared-string table lookup is now length-aware and no longer depends on NUL
termination. `sfindblock()` compares by `(SIZE == len && memcmp(...))`, hashing uses
`whashstr(s, end, ...)` through `StrHashN`, and lookup/insert APIs
(`findstring`, `make_shared_string`) avoid temporary NUL-terminated copies.

## Status

| Stage | Status |
|---|---|
| Rename `unused` → `blkend`, wire on all malloc create/resize/unlink paths | complete |
| `COUNTED_STRLEN` O(1) long-string path via `blkend` | complete |
| Shared-string length cap below USHRT_MAX | complete |
| Shared-string table supports non-NUL-terminated keys (`StrHashN`/`findblockn`) | complete |
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
| `ref_string(shared_str_t)` | `STRING_SHARED` payload | Uses `BLOCK(str)`, verifies via `findblockn` |
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

### Path to full compile-time enforcement (TODO)

When `STRING_TYPE_SAFETY` is defined, `shared_str_t` and `malloc_str_t` will
become opaque struct pointer types, turning contract violations at boundary
functions into hard compile errors.  Currently they remain transparent
`typedef char *` aliases; the migration must happen before the opaque form can
be enabled.

**Mechanical change to `stralloc.h`** — replace the transparent aliases with
forward-declared opaque pointer types, gated on `STRING_TYPE_SAFETY`:

```c
#ifdef STRING_TYPE_SAFETY
  struct _shared_str_opaque;
  struct _malloc_str_opaque;
  typedef struct _shared_str_opaque *shared_str_t;
  typedef struct _malloc_str_opaque *malloc_str_t;
  /* Escape hatches for call sites that genuinely need a raw char * */
  #define SHARED_STR_P(x)  ((char *)(x))
  #define MALLOC_STR_P(x)  ((char *)(x))
  #define TO_SHARED_STR(x) ((shared_str_t)(x))
  #define TO_MALLOC_STR(x) ((malloc_str_t)(x))
#else
  typedef char *shared_str_t;
  typedef char *malloc_str_t;
  #define SHARED_STR_P(x)  (x)
  #define MALLOC_STR_P(x)  (x)
  #define TO_SHARED_STR(x) (x)
  #define TO_MALLOC_STR(x) (x)
#endif
```

`svalue_u` gets two typed members alongside `u.string`:

```c
union svalue_u {
    char        *string;        /* generic / STRING_CONSTANT access */
    shared_str_t shared_string; /* STRING_SHARED payload */
    malloc_str_t malloc_string; /* STRING_MALLOC payload */
    ...
};
```

With opaque types these three are distinct to the compiler even though they
occupy the same storage.  Code that knows the subtype can use the typed member
for a free compile-time assertion; generic code continues using `u.string`.

**Migration order** — work from the smallest boundary outward.

1. `stralloc.h` / `stralloc.c` — make typedefs opaque, wire cast macros
   throughout, verify `STRING_TYPE_SAFETY=ON` build.
2. `svalue_u` — add `shared_string` / `malloc_string` members; no callers
   change yet.
3. Boundary storage sites — use typed members in subtype-specific assignments:
   - `push_shared_string` / `push_malloced_string` in [src/stack.c](../../src/stack.c)
   - `free_string_svalue` / `unlink_string_svalue` shared and malloc branches
     in [src/stralloc.c](../../src/stralloc.c)
   - The `EXTEND_SVALUE_STRING` / `SVALUE_STRING_JOIN` macros in
     [src/interpret.h](../../src/interpret.h) (already gate on
     `subtype == STRING_MALLOC`)
4. High-traffic efun sites — migrate `u.string` writes to typed members where
   the subtype is already known at the assignment point.
5. Generic read sites (`strcmp`, output, NUL-reliant paths) — keep `u.string`;
   these are catalogued in the NUL-termination section and are a separate future
   concern.

**Key constraint**: `u.string` must remain available as the generic view because
many legitimate call sites do not know the subtype (comparisons, tracing, output).
The typed members supplement it; they do not replace it.

## Paths still relying on NUL termination

These are partially addressed. Shared-string table lookup/insert now supports
non-NUL-terminated keys, but end-to-end runtime behavior is still NUL-dependent in
many VM and efun paths.

### Shared-string table (updated)
| Location | Reason |
|---|---|
| [src/stralloc.c `sfindblock`](../../src/stralloc.c) | Addressed: length-aware compare via `SIZE` + `memcmp`. |
| [src/stralloc.c `findstring` / `make_shared_string`](../../src/stralloc.c) | Addressed: span-aware lookup (`const char *s, const char *end`) without temporary NUL copy. |
| [src/stralloc.c trace logs](../../src/stralloc.c) | Updated: tracing now prints pointer addresses (`%p`) instead of `%s` to avoid truncation/over-read on non-NUL payloads. |

### Malloc string creation and copy
| Location | Reason |
|---|---|
| [src/stralloc.c `int_string_copy`](../../src/stralloc.c) | Measures source with `strlen`. |
| [src/stralloc.c `int_alloc_cstring`](../../src/stralloc.c) | Plain `strdup`-equivalent; no block header. |
| [src/stralloc.c `unlink_string_svalue` STRING_SHARED branch](../../src/stralloc.c) | Uses `strncpy`; this is still NUL-sensitive and can lose bytes after embedded `\0`. |

## TODO: Non-NUL string flow from LPC operators and efuns

The shared-string table can now intern and find byte spans with embedded NUL, but
LPC operators and efuns mostly still consume/produce C strings. To make non-NUL
strings usable from LPC execution paths, add span-aware plumbing in these stages:

1. Operators in [lib/lpc/operator.c](../../lib/lpc/operator.c):
    - Add/route `(start, end)` spans for substring/concatenation paths instead of
      relying on `strlen`/`strcpy`.
    - Ensure empty/non-empty slice constructors preserve explicit lengths.
2. Interpreter string macros in [src/interpret.h](../../src/interpret.h):
    - Replace `strlen`/`strcpy` usage in `EXTEND_SVALUE_STRING`,
      `SVALUE_STRING_ADD_LEFT`, and `SVALUE_STRING_JOIN` with explicit-length copies.
3. String efuns in [lib/efuns/string.c](../../lib/efuns/string.c) and related efuns:
    - Introduce length-aware variants internally for compare/search/case operations.
    - Preserve existing LPC-visible behavior where text semantics are required.
4. API convergence:
    - Standardize internal helpers around `(const char *s, size_t len)` or
      `(start, end)` forms, and call `make_shared_string(s, end)`/`findstring(s, end)`
      at boundaries.
5. Tests:
    - Add LPC-level regression tests that build strings containing embedded `\0`
      and validate operator + efun behavior matches documented contracts.

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
- Transparent `typedef char *` aliases enforce nothing at compile time today; they
  serve as documentation and as the named migration target for when the typedefs
  become opaque under `STRING_TYPE_SAFETY`.  The runtime guards (Layer 2) are
  the primary enforcement mechanism in the interim.  See the TODO migration plan
  in "Path to full compile-time enforcement" for the staged upgrade checklist.
