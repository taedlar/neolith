# Agent Development Guide

## Scope

- [AGENTS.md](../AGENTS.md) is the canonical repository map for file locations, build/run/test workflows, and documentation policy.
- This file stays compact and focuses on code-level invariants that should be enforced while editing driver/runtime code.

## Core Invariants

### Applies and Object Lifecycle
- Driver-to-LPC applies (`heart_beat()`, `reset()`, `init()`, etc.) dispatch through APPLY_CALL (don't care about return value) or APPLY_SLOT_CALL in [src/apply.h](../src/apply.h); see [docs/applies/](../docs/applies/).
- Object lifecycle invariants:
  - load compiles a base object (no `#` suffix)
  - clone adds `#N` and shares program
  - recompiles can coexist with older programs
  - destruction frees programs only when refcount reaches zero
- After applies, always re-check `ob->flags & O_DESTRUCTED`; the callee may self-destruct.
- Apply paths must leave the stack balanced even on failure.

### Build and Efun Integration
- Dependencies are tiered with clear layering:
  - **Driver layer (foundation)**: sets up subsystems, maintains `mud_state()`, and provides the runtime environment for LPC and efuns (plus `stem` integration used by unit tests).
  - **LPC layer**: built on top of the Driver layer.
  - **Efuns layer**: built on top of both the Driver and LPC layers.
- Efuns are generated, not manually registered:
  1. Update [lib/lpc/func_spec.c.in](../lib/lpc/func_spec.c.in)
  2. Build to produce `func_spec.i`
  3. Let `edit_source` regenerate dispatch tables
  4. Implement guarded C++(preferred) or C code in [lib/efuns/](../lib/efuns/)
- Generated artifacts such as `grammar.c`/`grammar.h` and efun tables from `edit_source` are regenerated, not edited directly.

## Coding Conventions

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
- Avoid line-wrapping churn; use LOC checks only for deliberate cleanup.

## Best Practices

### Integer Rules
- LPC runtime numeric values are `int64_t`.
- Print runtime integers with `PRId64`.
- Do not use `long` for LPC/runtime-sized values.
- Use `int` for small counters/indices when size is clearly bounded.
- Keep binary compatibility in mind: bump `driver_id` in [binaries.cpp](../lib/lpc/program/binaries.cpp) when opcode ordering or runtime struct sizes change.

### C++ svalue Idioms (Preferred)
- In C++ code, prefer `lpc::svalue` (owning), `lpc::svalue_view` / `lpc::const_svalue_view` (borrowing), and `lpc::svalue_ref` (retained-copy reference) over direct `svalue_t` union access.
- Use owning setters on `lpc::svalue` for writes; avoid mutating raw union fields directly.
- Prefer `lpc::const_svalue_view` for read-only paths and assertions; use mutable views only when mutation is required.
- Use `lpc::svalue_ref` when a retained-copy reference to borrowed `svalue_t` is required.
- In tests, assert through view APIs (`is_*`, `c_str()`, `number()`, etc.) rather than direct union access.
- For legacy C interop, use `assign_svalue_no_free(dest, src)` with `const svalue_t *from`.

### Runtime Safety
- When temporary allocations can unwind through LPC errors or apply calls, wrap them in `NEOLITH_HEAP_SCOPE(scope)` and release ownership explicitly with `NEOLITH_HEAP_RELEASE(ptr)` when it escapes.
- Guard state-dependent operations with `mud_state() >= MS_PRE_MUDLIB`; provide a fallback in compile-only or error paths.

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

