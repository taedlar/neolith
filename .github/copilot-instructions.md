# Agent Development Guide

## Project Overview
Neolith is a minimalist LPMud driver forked from MudOS v22pre5, modernizing decades-old C/C++ code while maintaining compatibility with the LPC (Lars Pensjö C) scripting language used by MUD builders.

**Development Priorities**: Stability, LPC compatibility, documentation, and incremental modernization (Boost, OpenSSL, CURL). See [docs/internals/](docs/internals/) for persistent architecture and design references.

**When adding features or refactoring**: Prioritize decisions that preserve LPC behavior and performance, favor portable C++ (Linux + Windows/MSVC/Clang), and maintain the single-threaded backend design.

## Key File Locations

**Build & Runtime**
- [config.h.in](config.h.in) — compile-time feature flags
- [src/neolith.conf](src/neolith.conf) — runtime configuration template
- [examples/m3_mudlib/](examples/m3_mudlib/) — test mudlib

**Core Source** (frequently modified)
- [src/backend.c](src/backend.c) — main event loop; [src/interpret.c](src/interpret.c) — LPC VM; [src/simulate.c](src/simulate.c) — object management
- [src/comm.c](src/comm.c) — network I/O; [src/apply.c](src/apply.c) — LPC apply dispatch
- [lib/efuns/func_spec.c](lib/efuns/func_spec.c) — efun definitions (code-generated, do not edit generated output)
- [lib/lpc/grammar.y](lib/lpc/grammar.y) — LPC parser grammar

**Reference Docs** (ground truth for LPC behavior)
- [docs/efuns/](docs/efuns/) — efun signatures and behavior
- [docs/applies/](docs/applies/) — driver-to-LPC apply callback reference

**Architecture Docs** (read before touching a subsystem)
- [docs/internals/lpc-types.md](docs/internals/lpc-types.md) — compile-time vs runtime type systems
- [docs/internals/lpc-program.md](docs/internals/lpc-program.md) — compiler memory layout and lifecycle
- [docs/internals/async-library.md](docs/internals/async-library.md) — async worker/queue/runtime design
- [docs/manual/dev.md](docs/manual/dev.md) — developer setup, build, and run guide

**Planning & History** (active work context)
- [docs/internals/](docs/internals/) — persistent feature architecture and subsystem design references
- [docs/history/](docs/history/) — active implementation reports (recent changes, may cause regressions)
- [docs/ChangeLog.md](docs/ChangeLog.md) — release-level change summaries

## Architecture

Neolith is an LPC VM driver with these core parts:

1. **Backend** ([src/backend.c](src/backend.c)) — main loop, timers, lifecycle orchestration.
2. **Interpreter** ([src/interpret.c](src/interpret.c)) — opcode execution and call-stack runtime.
3. **Simulate** ([src/simulate.c](src/simulate.c)) — object loading, cloning, movement, destruction.
4. **Comm** ([src/comm.c](src/comm.c)) — non-blocking network I/O and input/output buffering.
5. **LPC Compiler** ([lib/lpc/](lib/lpc/)) — on-demand LPC compile pipeline.
6. **Efuns** ([lib/efuns/](lib/efuns/)) — generated built-in LPC functions.

Keep this section as a map only. Put subsystem behavior and invariants in dedicated docs for retrieval:
- [docs/internals/async-library.md](docs/internals/async-library.md)
- [docs/internals/lpc-types.md](docs/internals/lpc-types.md)
- [docs/internals/lpc-program.md](docs/internals/lpc-program.md)
- [docs/applies/](docs/applies/)

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
- Use [src/neolith.conf](src/neolith.conf) as the config template.
- `-p` enables pedantic mode (memory leak checks); see [docs/manual/dev.md](docs/manual/dev.md).
- `-t` enables trace logging; see [docs/manual/trace.md](docs/manual/trace.md).

#### Running in Console Mode
- Use `-c` for stdin/stdout-driven testing (no telnet client required).
- Reference: [docs/manual/console-mode.md](docs/manual/console-mode.md).
- Example:
```bash
/path/to/neolith -f /path/to/neolith.conf -c < /path/to/console_commands.txt > /path/to/console_output.log
```

### Testing
- Unit tests use GoogleTest in [tests/](tests/); test files follow `test_*.cpp` and use `TEST()` / `TEST_F()`.
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

#### Testing complex interactions with a testing robot
- Use [examples/testbot.py](examples/testbot.py) to script multi-user interaction tests.
- Setup:
```bash
python -m venv .venv
.venv\Scripts\activate.bat  # Windows
source .venv/bin/activate  # Linux/WSL
python -m pip install pexpect
```

- Run:
```bash
cd examples
python testbot.py
```

## Code Conventions & Patterns

### Architecture and Modularity
- Keep subsystems in their libraries under [lib/](lib/) (`async`, `lpc`, `efuns`, etc.); driver glue lives in [src/](src/) and links through `stem`.
- Prefer `static` file-local state over new globals.
- Preserve init/deinit symmetry (`init_*()` with matching `deinit_*()`), coordinated by `get_machine_state()`.

### Includes and Headers
- In `.c` files, include `config.h` first.
- In [src/](src/) `.c` files, include `std.h` immediately after `config.h`.
- Keep headers C/C++ compatible (`extern "C"` guards) and avoid unnecessary includes.

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
- Current single call site is `do_comm_polling()` in [src/comm.c](src/comm.c); keep it that way.
- Workers should notify via `async_runtime_post_completion()`.
- Reference: [docs/internals/async-library.md](docs/internals/async-library.md).

### Applies and Object Lifecycle
- Driver-to-LPC applies (`heart_beat()`, `reset()`, `init()`, etc.) dispatch through `apply_low()` in [src/apply.c](src/apply.c); see [docs/applies/](docs/applies/).
- Object lifecycle invariants:
  - load compiles a base object (no `#` suffix)
  - clone adds `#N` and shares program
  - recompiles can coexist with older programs
  - destruction frees programs only when refcount reaches zero

### Build and Efun Integration
- Respect library dependency flow in [src/CMakeLists.txt](src/CMakeLists.txt):
  - `stem -> efuns, lpc, rc, socket, misc, logger, port`
  - `lpc -> logger, efuns, rc`
  - `efuns -> port, misc`
- Efuns are generated, not manually registered:
  1. Update [lib/efuns/func_spec.c](lib/efuns/func_spec.c)
  2. Build to produce `func_spec.i`
  3. Let `edit_source` regenerate dispatch tables
  4. Implement guarded C code in [lib/efuns/](lib/efuns/)

### LPC Types and Compiler Touchpoints
- Keep compile-time `TYPE_*` (`lpc_type_t`) separate from runtime `T_*` (`svalue_type_t`). Never mix domains.
- When checking compile-time base type, mask modifiers: `type & ~NAME_TYPE_MOD`.
- Arrays/classes use modifier checks (`TYPE_MOD_ARRAY`, `IS_CLASS(type)`).
- Type-system reference: [docs/internals/lpc-types.md](docs/internals/lpc-types.md).
- Compiler work reference: [docs/internals/lpc-program.md](docs/internals/lpc-program.md).
- Common edits:
  - opcode: `function.h` -> `icode.c` -> [src/interpret.c](src/interpret.c) -> `disassemble.c`
  - grammar: [lib/lpc/grammar.y](lib/lpc/grammar.y) -> `parse_trees.c` -> `icode.c`
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
- Write docs in Markdown under [docs/](docs/) (GitHub Flavored Markdown).
- Keep **permanent-state docs** in [docs/manual/](docs/manual/) and [docs/internals/](docs/internals/) accurate as code evolves.
  - Manuals are for operators and mudlib developers (coding in LPC); internals are for driver developers (coding in C/C++).
  - **docs/manual style**: Avoid explaining driver implementations in C/C++ terms; focus on behavior, contracts, and examples in LPC terms.
  - **docs/manual style**: Use code snippets to illustrate behavior, not to document implementation.
  - **docs/internals style**: Emphasize design rationale (why), architecture, and integration patterns; avoid duplicating implementation details.
  - **docs/internals style**: Link to source instead of copying code.
- **IMPORTANT**: Start feature work with a plan document in [docs/plan/](docs/plan/) to track design decisions, implementation status, and handoff instructions. Update the plan as work progresses and archive when complete.
  - Write a short description (< 300 words) of the plan at the top of the plan document.
  - Use staged status (`not started`, `in progress`, `complete`) when work spans multiple stages. Keep the status updated and visible following the description.
  - If the a stage contains a long checklist, ask for user confirmation before breaking it down into sub-stages with their own status tracking.
  - **NO PERMANENT CROSS REFERENCES**: plan documents must be treated as temporary that will be archived or deleted in a future commit. Do not link to them from permanent docs or source code.
  - Permanent-state docs can be created during implementation only after user confirmation, and must be kept concise and focused on behavior and contracts, not implementation details. Permanent docs should not link to plan documents.
  - **HANDOFFS** after implementation starts, plan documents should include clear handoff instructions in a top-level heading "Current State Handoff" near top of the document. Keep handoffs up-to-date whenever implementation status changes.
  - **LESSONS LEARNED**: add a "Lessons Learned" section to the plan document when implementation is completed with insights that may be useful for this plan. Place this section after handoffs and read it before implementing things.
- When you are required to archive a plan:
  - Ask for user confirmation before archiving any plan document. Show descriptive information, not the archiving action.
  - To archive a plan, add the plan doc to `hN.zip` in [docs/history/](docs/history/) and roll over to a new archive when the current one exceeds 1 MB. Delete the original plan doc after archiving.

### Documentation Best Practices
1. Keep docs concise and structured for retrieval (clear headings, tables, and short bullet lists).
2. Document decisions and interfaces, not full implementations; link to source for details.
3. Keep implementation reports focused on deltas and status.
4. Before PRs, remove outdated or redundant text and ensure plan/current-state docs are aligned.

## Reference Documentation

Documentation files use lowercase names with dash (`-`) separators, prefixed with the library/feature name. See [docs/file-organization.md](docs/file-organization.md) for the complete guide to doc placement and conventions.

## Common Pitfalls
1. **Don't modify generated files** like `grammar.c`/`grammar.h` (from Bison) or efun tables (from edit_source)
2. **Object destruction**: Always check `ob->flags & O_DESTRUCTED` after applies—objects can self-destruct
3. **Stack discipline**: Applies must clean up arguments even on failure (see [apply.c](src/apply.c) comments)
4. **Global state**: Minimize globals; use `static` within .c files when possible
5. **Line-of-code metrics**: Avoid unnecessary line wrapping; check LOC with `git ls-files | egrep -v '^(docs|examples)' | xargs wc -l`
6. **Type system mixing**: Never mix compile-time TYPE_* with runtime T_* values—see [lpc-types.md](docs/internals/lpc-types.md)
7. **Binary compatibility**: Always bump driver_id in [binaries.c](lib/lpc/program/binaries.c) when adding/removing/reordering opcodes or changing runtime struct sizes

## Agent Execution Priorities
- Prioritize impact-first changes: fix code and tests first, then update only docs directly affected by the change.
- Run the smallest relevant test scope for touched behavior (targeted test files first; expand scope only when risk is broader).
- Apply doc updates conditionally:
  - Update [docs/efuns/](docs/efuns/) only when efun behavior/signature is added or changed.
  - Update [docs/manual/](docs/manual/) or [docs/internals/](docs/internals/) only when architecture, contracts, or operational behavior changes.
  - Update [docs/ChangeLog.md](docs/ChangeLog.md) for release-relevant user-visible or developer-facing changes.
- Keep doc edits concise and source-linked; avoid restating implementation that is already clear in code.
- Use [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) as the policy source of truth when guidance conflicts.
