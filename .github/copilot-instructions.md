# Agent Development Guide

## Architecture

Neolith is an LPC VM driver with these core parts:

1. **Backend** ([src/backend.c](../src/backend.c)) — main loop, timers, lifecycle orchestration.
2. **Interpreter** ([src/interpret.c](../src/interpret.c)) — opcode execution and call-stack runtime.
3. **Simulate** ([src/simulate.c](../src/simulate.c)) — object loading, cloning, movement, destruction.
4. **Comm** ([src/comm.c](../src/comm.c)) — non-blocking network I/O and input/output buffering.
5. **LPC Compiler** ([lib/lpc/](../lib/lpc/)) — on-demand LPC compile pipeline.
6. **Efuns** ([lib/efuns/](../lib/efuns/)) — generated built-in LPC functions.

Keep this section as a map only. Put subsystem behavior and invariants in dedicated docs for retrieval:
- [docs/internals/](../docs/internals/) — persistent feature architecture and subsystem design references
- [docs/internals/async-library.md](../docs/internals/async-library.md)
- [docs/internals/lpc-types.md](../docs/internals/lpc-types.md)
- [docs/internals/lpc-program.md](../docs/internals/lpc-program.md)
- [docs/applies/](../docs/applies/) — driver-to-LPC apply callback reference
- [docs/manual/dev.md](../docs/manual/dev.md) — developer setup, build, and run guide

## Code Conventions & Patterns

### Architecture and Modularity
- Keep subsystems in their libraries under [lib/](../lib/) (`logger`, `async`, `lpc`, `efuns`, etc.); driver glue lives in [src/](../src/) and links through `stem`.
- Prefer `static` file-local state over new globals.
- Preserve init/deinit symmetry (`init_*()` with matching `deinit_*()`), coordinated by `mud_state()`.

### Includes and Headers
- In `.c` files, include `config.h` first.
- In [src/](../src/) `.c` files, include `std.h` immediately after `config.h`. Define `NO_STEM` before including `std.h` if the file is a low-level subsystem that should not depend on stem APIs.
- Keep headers C/C++ compatible (`extern "C"` guards when `__cplusplus` is defined) and avoid unnecessary includes.
- For standard C headers, use CMake to detect platform availability and wrap in `port/wrapper.h` for consistent inclusion.
- For subsystem headers, only include what is necessary for the .c file; avoid including headers that are not directly needed.

### Formatting
- C uses K&R style; C++ uses Stroustrup style.
- Use 2 spaces, no tabs.
- Keep opening braces on the same line.
- Do not cuddle `else` / `else if`; put `else` on a new line.
- Use spaces in control expressions (`if (x)`), one blank line between logical blocks, and two between functions.

### Integer Rules
- LPC runtime numeric values are `int64_t`.
- Print runtime integers with `PRId64`.
- Do not use `long` for LPC/runtime-sized values.
- Use `int` for small counters/indices when size is clearly bounded.

### C++ svalue Idioms (Preferred)
- In C++ code, prefer `lpc::svalue` (owning), `lpc::svalue_view` / `lpc::const_svalue_view` (borrowing), and `lpc::svalue_ref` (retained-copy reference) over direct `svalue_t` union access.

**Construction and ownership:**
- `lpc::svalue sv;` — default: initializes to `T_NUMBER 0`
- `lpc::svalue sv(other);` — copy: retained-copy (increments refcounts)
- `lpc::svalue sv(std::move(other));` — move: ownership transfer
- `lpc::svalue_ref ref(raw_sv_ptr);` — explicit retained-copy reference to borrowed `svalue_t`

**Setting values (use owning setters on `lpc::svalue`):**
- `sv.set_shared_string(s);` — frees old payload, adopts shared string
- `sv.set_malloc_string(s);` — frees old payload, adopts malloc string
- `sv.set_constant_string(s);` — frees old payload, sets constant string
- `sv.set_number(n);`, `sv.set_real(d);`, `sv.set_object(ob);`, `sv.set_array(arr);` — frees old, sets new

**Borrowing and viewing:**
- `auto view = sv.view();` or `lpc::svalue_view::from(&raw_sv);` — mutable view over borrowed svalue
- `auto view = sv.view() const` or `lpc::const_svalue_view::from(&raw_sv);` — immutable view
- Prefer immutable views (`const_svalue_view`) in const contexts; avoid `const_cast` patterns
- `view()` as zero-cost borrow-only access; never mutate across borrow boundaries

**Accessing values (via views):**
- Type checks: `view.is_string()`, `view.is_number()`, `view.is_array()`, etc.
- String accessors: `view.c_str()`, `view.length()` (O(1) for counted strings)
- Typed getters: `view.shared_string()`, `view.malloc_string()`, `view.const_string()`, `view.number()`, `view.object()`, etc.

**In tests and helpers:**
- Use owning setters: `target.set_malloc_string(str);` instead of `svalue_view::from(&target).set_malloc_string(str);` to avoid manual refcount management
- Assert via view APIs: `auto v = sv.view(); EXPECT_TRUE(v.is_string()); EXPECT_STREQ(v.c_str(), "expected");`
- Avoid direct union member access (`sv.raw()->u.string`, etc.) unless unavoidable for legacy C paths

**Legacy C API interop:**
- `assign_svalue_no_free(dest, src);` — shallow copy + retain payload (both source and dest must be writable C structs)
- Source is now const: `const svalue_t *from` (no const_cast needed)

### Socket Descriptor Rules (Winsock Compatibility)
- Distinguish LPC socket IDs from native OS socket descriptors:
  - LPC socket IDs (driver-level values returned by `socket_create()` and consumed by `socket_connect()` / `socket_close()` / `get_socket_operation_info()`) use `int`.
  - Native OS socket descriptors (values used with `socket()`, `accept()`, `select()`, `SOCKET_CLOSE`, `INVALID_SOCKET_FD`) use `socket_fd_t`.
- Preserve API contract types: do not change LPC socket-ID function signatures from `int` to `socket_fd_t`.
- Use naming that encodes the distinction:
  - LPC socket ID variables/parameters: `socket_id`, `fd` (when clearly driver-level), `lpc_socket_id`.
  - Native descriptor variables/parameters: `listener_fd`, `accepted_fd`, `native_fd`, `tracked_fd`.
- In mixed-scope code (especially tests), avoid reusing one variable for both domains; keep one `int` LPC ID and one `socket_fd_t` native descriptor with distinct names.

### Async Runtime (Critical)
- `async_runtime_wait()` must have exactly one caller thread: the backend/main thread.
- Never call `async_runtime_wait()` from workers or concurrently.
- Current single call site is `do_comm_polling()` in [src/comm.c](../src/comm.c); keep it that way.
- Workers should notify via `async_runtime_post_completion()`.
- Reference: [docs/internals/async-library.md](../docs/internals/async-library.md).

### Applies and Object Lifecycle
- Driver-to-LPC applies (`heart_beat()`, `reset()`, `init()`, etc.) dispatch through APPLY_CALL (don't care about return value) or APPLY_SLOT_CALL in [src/apply.h](../src/apply.h); see [docs/applies/](../docs/applies/).
- Object lifecycle invariants:
  - load compiles a base object (no `#` suffix)
  - clone adds `#N` and shares program
  - recompiles can coexist with older programs
  - destruction frees programs only when refcount reaches zero

### Build and Efun Integration
- Respect library dependency flow in [src/CMakeLists.txt](../src/CMakeLists.txt):
  - `stem -> efuns, lpc, rc, socket, misc, logger, port`
  - `lpc -> logger, efuns, rc`
  - `efuns -> port, misc`
- Efuns are generated, not manually registered:
  1. Update [lib/lpc/func_spec.c.in](../lib/lpc/func_spec.c.in)
  2. Build to produce `func_spec.i`
  3. Let `edit_source` regenerate dispatch tables
  4. Implement guarded C++(preferred) or C code in [lib/efuns/](../lib/efuns/)

### LPC Types and Compiler Touchpoints
- Keep compile-time `TYPE_*` (`lpc_type_t`) separate from runtime `T_*` (`svalue_type_t`). Never mix domains.
- When checking compile-time base type, mask modifiers: `type & ~NAME_TYPE_MOD`.
- Arrays/classes use modifier checks (`TYPE_MOD_ARRAY`, `IS_CLASS(type)`).
- Type-system reference: [docs/internals/lpc-types.md](../docs/internals/lpc-types.md).
- Compiler work reference: [docs/internals/lpc-program.md](../docs/internals/lpc-program.md).
- Common edits:
  - opcode: `function.h` -> `icode.c` -> [src/interpret.c](../src/interpret.c) -> `disassemble.c`
  - grammar: [lib/lpc/grammar.y](../lib/lpc/grammar.y) -> `parse_trees.c` -> `icode.c`
  - compile debug: `TT_COMPILE`, `opt_trace(...)`, `num_parse_error`, `mem_block[].current_size`

## Common Pitfalls
1. **Don't modify generated files** like `grammar.c`/`grammar.h` (from Bison) or efun tables (from edit_source)
2. **Object destruction**: Always check `ob->flags & O_DESTRUCTED` after applies—objects can self-destruct
3. **Stack discipline**: Applies must clean up arguments even on failure (see [apply.cpp](../src/apply.cpp) comments)
4. **Global state**: Minimize globals; use `static` within .c files when possible
5. **Line-of-code metrics**: Avoid unnecessary line wrapping; check LOC with `git ls-files | egrep -v '^(docs|examples)' | xargs wc -l`
6. **Type system mixing**: Never mix compile-time TYPE_* with runtime T_* values—see [lpc-types.md](../docs/internals/lpc-types.md)
7. **Binary compatibility**: Always bump driver_id in [binaries.cpp](../lib/lpc/program/binaries.cpp) when adding/removing/reordering opcodes or changing runtime struct sizes
8. **svalue wrapper construction**: Do not assign raw `svalue_t` to `lpc::svalue`. Use `lpc::svalue_ref` for explicit retained-copy semantics, or `lpc::svalue::view()` for borrowing-only access. The raw constructor was removed to prevent ambiguity.
9. **svalue borrowing**: Use `lpc::svalue_view` or `lpc::const_svalue_view` for zero-cost borrow-only access to `svalue_t` fields. Prefer immutable views (`const_svalue_view`) when mutation is not needed.
10. **Temporary allocation unwinding in C/C++**: Manual *ALLOC (DMALLOC, DXALLOC, etc.) in C/C++ functions can leak when exceptions (LPC errors, apply calls, or function pointer invocations) unwind before cleanup. **Recommendation**: Migrate allocation-heavy functions to C++ and wrap temporary blocks with `NEOLITH_HEAP_SCOPE(scope)`. If tracked ownership escapes the scope, untrack it with `NEOLITH_HEAP_RELEASE(ptr)` before returning or storing it. The RAII scope guard in [src/malloc.cpp](../src/malloc.cpp) frees tracked allocations on unwind, while `NEOLITH_HEAP_RELEASE` prevents accidental frees on ownership transfer. See [lib/efuns/command.cpp](../lib/efuns/command.cpp) and [lib/efuns/objects.cpp](../lib/efuns/objects.cpp) for patterns. For C-only legacy code where migration is not feasible, explicitly free allocations on all early-return and error paths before unwinding.
11. **Guarding driver state with mud_state()**: Many subsystems are initialized only after `MS_PRE_MUDLIB`. Calling state-dependent functions (for example, stack operations like `share_and_push_string()` / `copy_and_push_string()`, master applies, or evaluator operations) without checking `mud_state()` first can crash or produce undefined behavior. **Pattern**: In compile-only paths (for example, `compile_file()` without simulate setup) or error handlers, guard such operations with `if (mud_state() >= MS_PRE_MUDLIB) { /* operation */ } else { /* fallback */ }`. Example: `smart_log()` in [src/stem.cpp](../src/stem.cpp) checks state before pushing strings and falls back to `LOG_ERROR()` when mudlib is unavailable. **In unit tests**: Use `setup_simulate()` and `init_master()` to advance `mud_state()` before state-dependent operations, or use bare `compile_file()` only when mudlib state is intentionally not needed.
