# Byte-Span String Literals in LPC

Allow LPC source strings to contain embedded null bytes (`\0`, `\x00`). Today the lexer actively rejects them. The change threads byte-span ownership through three layers: the lexer/parser pipeline, the compiler string table, and the binary cache round-trip. A `T_STRING` svalue already carries a counted malloc/shared string with O(1) length, so the runtime itself requires no changes.

## Status

| Stage | Title | Status |
|-------|-------|--------|
| 1 | Lexer and compiler pipeline | complete |
| 2 | `save_binary` / `load_binary` round-trip | complete |
| 3 | Unit tests | complete |

## Current State Handoff

- Date: 2026-04-20
- Started implementation with the `load_binary` string-table fix in [lib/lpc/program/binaries.c](../../lib/lpc/program/binaries.c): `make_shared_string(buf, NULL)` changed to `make_shared_string(buf, buf + len)`.
- This removes `strlen` truncation for embedded-null bytes during binary load.
- Added `store_prog_string_len(const char *, size_t)` in [lib/lpc/compiler.c](../../lib/lpc/compiler.c) and switched binary string-switch patching to use explicit byte length (`SHARED_STRLEN`) in [lib/lpc/program/binaries.c](../../lib/lpc/program/binaries.c).
- `L_STRING` now carries byte-span data (`str`, `len`) in [lib/lpc/grammar.y](../../lib/lpc/grammar.y) and [lib/lpc/lex.c](../../lib/lpc/lex.c).
- Parser string concatenation rules now concatenate by explicit byte length (no `strlen`) in [lib/lpc/grammar.y](../../lib/lpc/grammar.y).
- Added binary round-trip regression coverage with embedded-null literals in [tests/test_lpc_compiler/test_save_binary.cpp](../../tests/test_lpc_compiler/test_save_binary.cpp) plus LPC fixture [examples/m3_mudlib/api/bytespan.c](../../examples/m3_mudlib/api/bytespan.c).
- Added lexer embedded-null coverage in [tests/test_lpc_lexer/test_lpc_lexer.cpp](../../tests/test_lpc_lexer/test_lpc_lexer.cpp).
- No binary-format changes were introduced.
- Remaining work focus: monitor follow-up refactors for generated parser/lexer artifacts and keep new byte-span tests in the default CI matrix.

## Lessons Learned

- The binary file format already stored string-table entries as length-prefixed bytes; the data-loss bug was solely in load-time reconstruction via `strlen`.
- Converting parser string production rules to span-aware concatenation removed most hidden `strlen` dependencies without touching runtime `svalue_t` layout.
- Hex escapes consume all subsequent hex digits by design (`strtol(..., 16)`), so tests should use non-hex suffix bytes when validating exact `\x00` behavior.

---

## Stage 1 — Lexer and Compiler Pipeline

### Problem

- `yylval.string` carries only a `char*`; consumers infer length via `strlen`.
- The lexer immediately rejects `\0` and `\x00` escape sequences with `yyerror` ([lib/lpc/lex.c](../../lib/lpc/lex.c) lines ~2068 and ~2095).
- `scratch_join` uses `strlen` for byte counting — not span-safe ([lib/misc/scratchpad.c](../../lib/misc/scratchpad.c)).
- `store_prog_string(const char *)` calls `make_shared_string(str, NULL)`, which falls back to `strlen` ([lib/lpc/compiler.c](../../lib/lpc/compiler.c) line 1184).
- The `NODE_STRING` constant-fold path in grammar.y uses `strlen`/`strcpy`/`strcat` to combine adjacent literals ([lib/lpc/grammar.y](../../lib/lpc/grammar.y) ~line 1476).

### Required Changes

#### 1a — Introduce `lpc_string_span` in YYSTYPE

Add a `{ char *ptr; size_t len; }` member to the Bison `%union` in [lib/lpc/grammar.y](../../lib/lpc/grammar.y) to carry byte-spans for `L_STRING` tokens.

Update `%token <span>` (or equivalent) for `L_STRING`.  Consumers of `$n` for `L_STRING` must switch from `char*` to the new member.

#### 1b — Lexer: remove null rejection, propagate length

In [lib/lpc/lex.c](../../lib/lpc/lex.c):
- Remove the two `yyerror("Embedded null …")` guards (the TODO comments are already there).
- Emit the null byte normally (`*to++ = (unsigned char)tmp`).
- All `return L_STRING` paths must set `yylval.span = { ptr, computed_len }` where `computed_len` is derived from the scratchpad allocation, not `strlen`.

`scratch_copy_string` and `scratch_copy` return pointers whose lengths must be tracked alongside the return value. Options:
- Store the length in the scratchpad header (2 bytes already reserved per entry as `*(s - 2)`) — but those bytes are currently used as a magic/flag field.
- Return a `{ char*, size_t }` pair from the scratchpad helpers (preferred; avoids reinterpreting header bytes).

#### 1c — `scratch_join`: byte-span-safe version

`scratch_join` in [lib/misc/scratchpad.c](../../lib/misc/scratchpad.c) uses `strlen` to compute the join point. It needs a `scratch_join_span(char*, size_t, char*, size_t)` variant (or take explicit lengths) that uses the supplied counts rather than `strlen`. The grammar rules `string_con1` / `string_con2` must pass lengths through.

#### 1d — `store_prog_string`: accept explicit length

Change signature from `store_prog_string(const char*)` to `store_prog_string(const char*, size_t)` and forward to `make_shared_string(str, str + len)`. `make_shared_string` already supports non-null-terminated input via its `end` pointer parameter.

All callers must supply the length. Most callers are passing driver-internal C strings (identifiers, names) that are guaranteed null-terminated — they can pass `strlen(str)`.  LPC literal sources come from the lexer span.

Update the `PROG_STRING(n)` macro declaration in [lib/lpc/compiler.h](../../lib/lpc/compiler.h) to reflect the new signature.

#### 1e — Grammar `NODE_STRING` combine path

The constant-fold in grammar.y that combines `NODE_STRING + NODE_STRING` uses:
```c
s_new = DXALLOC(strlen(s1) + strlen(s2) + 1, ...);
strcpy(s_new, s1); strcat(s_new + l, s2);
$$->v.number = store_prog_string(s_new);
```
Replace with explicit length variants: `memcpy` with lengths obtained from `SHARED_STRLEN(s1)` and `SHARED_STRLEN(s2)`.

#### 1f — `parse_trees.h` `CREATE_STRING` / `PROG_STRING_NODE` macro

The `CREATE_STRING(node, str)` helper that calls `store_prog_string(str)` must propagate the span length.

---

## Stage 2 — `save_binary` / `load_binary` Round-Trip

### Problem

- **Save** ([lib/lpc/program/binaries.c](../../lib/lpc/program/binaries.c) ~line 188): Uses `SHARED_STRLEN(p->strings[i])` — this is O(1) and span-safe for `STRING_SHARED` strings.  The on-disk format is already 16-bit length + raw bytes (no null terminator), so embedded nulls are preserved transparently in the write path. **Save path is already correct** once stage 1 produces span-correct shared strings.

- **Load** ([lib/lpc/program/binaries.c](../../lib/lpc/program/binaries.c) ~line 678): reads `len` bytes into `buf`, then calls `make_shared_string(buf, NULL)`.  `NULL` triggers `strlen` in `make_shared_string`, which truncates at the first embedded null.

### Required Change

In `READ_STRING_TABLE`, change:
```c
p->strings[i] = make_shared_string(buf, NULL);
```
to:
```c
p->strings[i] = make_shared_string(buf, buf + len);
```

No format change; no `driver_id` bump required. No other callers in the load path are affected.

---

## Stage 3 — Unit Tests

### 3a — Lexer tests ([tests/test_lpc_lexer/test_lpc_lexer.cpp](../../tests/test_lpc_lexer/test_lpc_lexer.cpp))

Add a `parseByteSpanStringLiteral` test:
- Input: `"Hello\0World"` — 11 bytes.
- Assert token is `L_STRING`.
- Assert `yylval.span.len == 11`.
- Assert bytes at offsets 0–4 equal `Hello`, byte at offset 5 is `\0`, bytes 6–10 equal `World`.
- Repeat for `\x00` variant.
- Assert adjacent literal concatenation: `"foo\0" "bar"` yields a single 7-byte span.

### 3b — Binary round-trip test ([tests/test_lpc_compiler/test_save_binary.cpp](../../tests/test_lpc_compiler/test_save_binary.cpp))

Add or extend `SaveBinaryRoundTrip` to use an LPC file whose string table contains an embedded-null literal. After `save_binary` + `load_binary`, verify the loaded `program_t` string entry via `SHARED_STRLEN` equals the original byte count and the byte contents match.

### 3c — Interpreter test ([tests/test_lpc_interpreter/](../../tests/test_lpc_interpreter/))

Add an inline LPC snippet test:
```lpc
string f() { return "ab\0cd"; }
```
After calling `f()`, assert via `lpc::svalue_view`:
```cpp
auto v = lpc::svalue_view::from(sp);
ASSERT_TRUE(v.is_string());
EXPECT_EQ(v.length(), 5u);
EXPECT_EQ(std::string(v.c_str(), v.length()), std::string("ab\0cd", 5));
```

---

## Implementation Notes

### Dependency Order

Stage 2 (load fix) is nearly independent and very small — one line change. It can be done first if a targeted fix is preferred before the larger lexer rework.

Stage 1 has internal ordering:
1. 1d (`store_prog_string` signature) + 1b (null byte emission) are the minimal "proof of concept".
2. 1a (YYSTYPE span) and 1c (`scratch_join`) are required for correct lengths to flow end-to-end.
3. 1e and 1f complete the compiler-pipeline coverage.

Stage 3 tests should be written incrementally alongside each stage sub-step to catch regressions early.

### `driver_id` Bump

The binary format does **not** change (already length-prefixed). No `driver_id` bump is needed.

### `STRING_CONSTANT` Subtype

`STRING_CONSTANT` strings are C-literal pointers from driver code and are never produced by LPC source. They are explicitly excluded from byte-span semantics in `SVALUE_STRLEN`. No changes required.

### `check_legal_string`

`check_legal_string` in [src/simulate.c](../../src/simulate.c) scans for illegal characters. Review whether a null byte should be legal or illegal in the contexts where it is called (file names, object names). These checks are on the output side and may legitimately remain strict — evaluate per call-site after stage 1 is complete.
