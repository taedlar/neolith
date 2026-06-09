# Coding Agent Instructions

## Key File Locations

**Build & Runtime**
- [config.h.in](config.h.in) — compile-time feature flags template; configured by CMake into `config.h`
- [src/neolith.conf](src/neolith.conf) — runtime configuration template
- [examples/m3_mudlib/](examples/m3_mudlib/) — test mudlib
- [examples/m3_testbots/](examples/m3_testbots/) — integration test scenarios simulating user interactions
- [examples/apps/](examples/apps/) — example applications using Neolith as a LPC shell

**Core Source** (frequently modified)
- [src/backend.c](src/backend.c) — main event loop; [src/interpret.c](src/interpret.c) — LPC VM; [src/simulate.c](src/simulate.c) — object management
- [src/comm.c](src/comm.c) — network I/O; [src/apply.cpp](src/apply.cpp) — LPC apply dispatch
- [lib/lpc/](lib/lpc/) — LPC compiler pipeline and runtime types
- [lib/lpc/func_spec.c.in](lib/lpc/func_spec.c.in) — efun definitions source template; edited directly, configured by CMake into `func_spec.c` then preprocessed into `func_spec.i`
- [lib/lpc/grammar.y](lib/lpc/grammar.y) — LPC parser grammar
- [lib/port/](lib/port/) — platform abstraction layer (file I/O, sockets, etc.)
- [lib/misc/](lib/misc/) — utilities (string, time, host filepath, etc.)

**Reference Docs** (ground truth for LPC behavior)
- [docs/efuns/](docs/efuns/) — efun signatures and behavior
- [docs/applies/](docs/applies/) — driver-to-LPC apply callback reference

**Planning & History** (active work context)
- [docs/plan/](docs/plan/) — active design and implementation plans (short-term, may be deleted or archived)
- [docs/history/](docs/history/) — recently completed plans and experimental features.
- [docs/ChangeLog.md](docs/ChangeLog.md) — release-level change summaries

## Architecture Layers and Runtime Contracts

- Layering model:
  - Driver layer is the foundation: it initializes subsystems, maintains `mud_state()`, and provides the runtime environment consumed by LPC and efuns (and `stem` integration for unit tests).
  - LPC layer is built on top of Driver.
  - Efuns layer is built on top of Driver and LPC.
- Object/apply safety:
  - After applies, re-check `ob->flags & O_DESTRUCTED` because targets may self-destruct.
  - Keep apply paths stack-balanced on both success and failure.
- Generated-file policy:
  - Do not edit generated artifacts directly (`grammar.c`/`grammar.h`, generated efun tables); edit the source templates and regenerate.

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
  cmake --build --preset ci-macos
  ```

### Running
- Basic run:
  ```bash
  /path/to/neolith -f /path/to/neolith.conf -p
  ```
- Use [examples/m3.conf](../examples/m3.conf) as a quick minimal runnable smoke-testing config.
  - Mudlib directory path is resolved as related path to `m3.conf`.
- `-p` enables pedantic mode (memory leak checks); see [docs/manual/dev.md](../docs/manual/dev.md).
- `-t` enables trace logging; see [docs/manual/trace.md](../docs/manual/trace.md).

#### Running in Console Mode (`-c`)
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
- Write docs in Markdown under [docs/](docs/) (GitHub Flavored Markdown). Follow [file-organization.md](docs/file-organization.md) for naming and structure.
- Name documentation files with lowercase letters and dashes (`-`) as separators (no underscores or camelCase), and prefix filenames with the library, feature, or subsystem (for example, `async-dns-integration.md`).
- Updating existing docs can proceed silently. Creation of new documents requires user approval.
- Start feature work with a plan document in [docs/plan/](docs/plan/) to track design decisions, implementation status, and handoff instructions, and keep it updated as work progresses.
- Search [docs/history](docs/history/) for recently completed plans when debugging regressions; they may contain bugs or experimental changes.
- Keep **permanent-state docs** in [docs/manual/](docs/manual/) and [docs/internals/](docs/internals/) accurate as code evolves:
  - Manuals are for operators and mudlib developers (coding in LPC); internals are for driver developers (coding in C/C++).
  - Avoid implementation details; focus on behavior, contracts, and design decisions.
  - Permanent-state docs can only link to other permanent-state docs (no links to plan docs or source code).
- When adding content to audience-specific docs (manuals vs internals), ensure the content matches the intended audience.
- Use [docs/manual/internals.md](docs/manual/internals.md) as a cross-reference for linking between manuals and internals when needed.

### Documentation Best Practices
1. Keep docs concise and structured for retrieval (clear headings, tables, and short bullet lists).
2. Don't mix driver-facing and mudlib-facing details in the same doc.
3. Document decisions and interfaces, not full implementations.
4. Keep implementation status focused on deltas.
5. When updating docs, remove outdated or redundant text and keep plan/current-state docs aligned.

## Agent Execution Priorities
- Prioritize impact-first changes: fix code and tests first, then update only docs directly affected by the change.
- Run the smallest relevant test scope for touched behavior (targeted test files first; expand scope only when risk is broader).
- Apply doc updates conditionally:
  - Update [docs/efuns/](../docs/efuns/) only when efun behavior/signature is added or changed.
  - Update [docs/manual/](../docs/manual/) or [docs/internals/](../docs/internals/) only when architecture, contracts, or operational behavior changes.
  - Update [docs/ChangeLog.md](../docs/ChangeLog.md) for release-relevant user-visible or developer-facing changes.
- Keep doc edits concise and source-linked; avoid restating implementation that is already clear in code.
- Use [docs/CONTRIBUTING.md](../docs/CONTRIBUTING.md) as the policy source of truth when guidance conflicts.
