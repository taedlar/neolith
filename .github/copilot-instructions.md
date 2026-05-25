# Agent Development Guide

## Project Overview
Neolith is a minimalist LPMud driver forked from MudOS v22pre5, modernizing decades-old C/C++ code while maintaining compatibility with the LPC (Lars Pensjö C) scripting language used by MUD builders.

**Development Priorities**: Stability, LPC compatibility, documentation, and incremental modernization (Boost, OpenSSL, CURL). See [docs/internals/](../docs/internals/) for persistent architecture and design references.

**When adding features or refactoring**: Prioritize decisions that preserve LPC behavior and performance, favor portable C++ (Linux + Windows/MSVC/Clang), and maintain the single-threaded backend design.

## Key File Locations

**Build & Runtime**
- [config.h.in](../config.h.in) — compile-time feature flags
- [src/neolith.conf](../src/neolith.conf) — runtime configuration template
- [examples/m3_mudlib/](../examples/m3_mudlib/) — test mudlib
- [examples/m3_testbots/](../examples/m3_testbots/) — integration test scenarios simulating user interactions
- [examples/apps/](../examples/apps/) — example applications using Neolith as a LPC shell

**Core Source** (frequently modified)
- [src/backend.c](../src/backend.c) — main event loop; [src/interpret.c](../src/interpret.c) — LPC VM; [src/simulate.c](../src/simulate.c) — object management
- [src/comm.c](../src/comm.c) — network I/O; [src/apply.cpp](../src/apply.cpp) — LPC apply dispatch
- [lib/lpc/](../lib/lpc/) — LPC compiler pipeline and runtime types
- [lib/lpc/func_spec.c.in](../lib/lpc/func_spec.c.in) — efun definitions source template; edited directly, configured by CMake into `func_spec.c` then preprocessed into `func_spec.i`
- [lib/lpc/grammar.y](../lib/lpc/grammar.y) — LPC parser grammar
- [lib/port/](../lib/port/) — platform abstraction layer (file I/O, sockets, etc.)
- [lib/misc/](../lib/misc/) — utilities (string, time, host filepath, etc.)

**Reference Docs** (ground truth for LPC behavior)
- [docs/efuns/](../docs/efuns/) — efun signatures and behavior
- [docs/applies/](../docs/applies/) — driver-to-LPC apply callback reference

**Planning & History** (active work context)
- [docs/plan/](../docs/plan/) — active design and implementation plans (short-term, may be deleted or archived)
- [docs/history/](../docs/history/) — recently completed plans and experimental features.
- [docs/ChangeLog.md](../docs/ChangeLog.md) — release-level change summaries

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

## Critical Developer Workflows

### Building
- Run from repository root (where `CMakePresets.json` lives).
- List presets:
  ```bash
  cmake --list-presets
  ```
- Configure + build pattern:
  ```bash
  cmake --preset <configure-preset>
  cmake --build --preset <build-preset>
  ```
- Preset prefixes:
  - `dev-`: incremental Debug builds
  - `pr-`: incremental RelWithDebInfo builds
  - `ci-`: clean RelWithDebInfo rebuilds
- Common clean build commands:
  ```bash
  cmake --build --preset ci-linux
  cmake --build --preset ci-vs16-x64
  cmake --build --preset ci-clang-x64
  ```

### Running
- Basic run:
  ```bash
  /path/to/neolith -f /path/to/neolith.conf -p
  ```
- Use [src/neolith.conf](../src/neolith.conf) as the config template.
- `-p` enables pedantic mode (memory leak checks); see [docs/manual/dev.md](../docs/manual/dev.md).
- `-t` enables trace logging; see [docs/manual/trace.md](../docs/manual/trace.md).

#### Running in Console Mode
- Use `-c` for stdin/stdout-driven testing (no telnet client required).
- Reference: [docs/manual/console-mode.md](../docs/manual/console-mode.md).
- Example:
```bash
/path/to/neolith -f /path/to/neolith.conf -c < /path/to/console_commands.txt > /path/to/console_output.log
```

### Testing
- Unit tests use GoogleTest in [tests/](../tests/); test files follow `test_*.cpp` and use `TEST()` / `TEST_F()`.
- Run `ctest` from repository root.

```bash
ctest --preset ut-linux
ctest --preset ut-vs16-x64
ctest --preset ut-clang-x64
```

- For targeted runs using `--test-dir`, include `--build-and-test` after code changes.
- Verify before running:
  - `--test-dir` matches the intended build output/platform
  - `-R` matches the intended configuration (for example, `RelWithDebInfo`)

Example:
```bash
ctest --test-dir out/build/clang-x64 -R RelWithDebInfo --build-and-test
```

#### Integrated Smoke Testing with M3 Testbot
- Use `examples/m3_testbots` for integration testing involving simulation of user interactions.
- Update `examples/m3_testbots/src/smoke_test.py` with test scenarios as needed.
- Run:
```bash
hatch run smoke_test
```

## Code Conventions & Patterns

### Architecture and Modularity
- Keep subsystems in their libraries under [lib/](../lib/) (`async`, `lpc`, `efuns`, etc.); driver glue lives in [src/](../src/) and links through `stem`.
- Prefer `static` file-local state over new globals.
- Preserve init/deinit symmetry (`init_*()` with matching `deinit_*()`), coordinated by `mud_state()`.

### Includes and Headers
- In `.c` files, include `config.h` first.
- In [src/](../src/) `.c` files, include `std.h` immediately after `config.h`.
- Keep headers C/C++ compatible (`extern "C"` guards when `__cplusplus` is defined) and avoid unnecessary includes.

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
  4. Implement guarded C code in [lib/efuns/](../lib/efuns/)

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

## Documentation Conventions

- Use inline Doxygen function comments:
  ```c
  /**
   * @brief Brief description.
   * @param arg Description of parameter.
   * @returns What the function returns.
   */
  ```
- Add block comments for complex logic or important invariants, especially in critical paths like memory management, applies, and async handling.
- Write docs in Markdown under [docs/](../docs/) (GitHub Flavored Markdown). Follow [file-organization.md](../docs/file-organization.md) for naming and structure.
- Name documentation files with lowercase letters and dashes (`-`) as separators (no underscores or camelCase), and prefix filenames with the library, feature, or subsystem (for example, `async-dns-integration.md`).
- Updating existing docs can proceed silently. Creation of new documents requires user approval.
- Start feature work with a plan document in [docs/plan/](../docs/plan/) to track design decisions, implementation status, and handoff instructions, and keep it updated as work progresses.
- Search [docs/history](../docs/history/) for recently completed plans when debugging regressions; they may contain bugs or experimental changes.
- Keep **permanent-state docs** in [docs/manual/](../docs/manual/) and [docs/internals/](../docs/internals/) accurate as code evolves:
  - Manuals are for operators and mudlib developers (coding in LPC); internals are for driver developers (coding in C/C++).
  - Avoid implementation details; focus on behavior, contracts, and design decisions.
  - Permanent-state docs can only link to other permanent-state docs (no links to plan docs or source code).
- When adding content to audience-specific docs (manuals vs internals), ensure the content matches the intended audience.
- Use [docs/manual/internals.md](../docs/manual/internals.md) as a cross-reference for linking between manuals and internals when needed.

### Documentation Best Practices
1. Keep docs concise and structured for retrieval (clear headings, tables, and short bullet lists).
2. Don't mix driver-facing and mudlib-facing details in the same doc.
3. Document decisions and interfaces, not full implementations.
4. Keep implementation status focused on deltas.
5. When updating docs, remove outdated or redundant text and keep plan/current-state docs aligned.

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

## Agent Execution Priorities
- Prioritize impact-first changes: fix code and tests first, then update only docs directly affected by the change.
- Run the smallest relevant test scope for touched behavior (targeted test files first; expand scope only when risk is broader).
- Apply doc updates conditionally:
  - Update [docs/efuns/](../docs/efuns/) only when efun behavior/signature is added or changed.
  - Update [docs/manual/](../docs/manual/) or [docs/internals/](../docs/internals/) only when architecture, contracts, or operational behavior changes.
  - Update [docs/ChangeLog.md](../docs/ChangeLog.md) for release-relevant user-visible or developer-facing changes.
- Keep doc edits concise and source-linked; avoid restating implementation that is already clear in code.
- Use [docs/CONTRIBUTING.md](../docs/CONTRIBUTING.md) as the policy source of truth when guidance conflicts.
