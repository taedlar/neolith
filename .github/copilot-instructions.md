# Neolith LPMud Driver AI Development Guide

## Project Overview
Neolith is a minimalist LPMud driver forked from MudOS v22pre5, modernizing decades-old C/C++ code while maintaining compatibility with the LPC (Lars Pensjö C) scripting language used by MUD builders.

**Commercial use is NOT ALLOWED** (GPLv2 + original author restrictions).

## Architecture: The Big Picture

The driver operates as a virtual machine for LPC scripts, organized into these major components:

1. **Backend** ([src/backend.c](src/backend.c)): Main event loop handling I/O, timers, and object lifecycle
   - Dispatches `heart_beat()` applies every 2 seconds for animated objects
   - Triggers `reset()` applies for lazy object initialization
   - Processes `call_out()` delayed function calls
   
2. **Interpreter** ([src/interpret.c](src/interpret.c)): Stack machine executing compiled LPC opcodes
   - Manages value stack and control stack for function calls
   - Routes calls through apply cache for performance
   - Handles simul_efuns (kernel-level LPC functions loaded from mudlib)

3. **Simulate** ([src/simulate.c](src/simulate.c)): Virtual world object management
   - Maps mudlib filesystem paths to LPC objects (e.g., `/std/room.c` → `/std/room`)
   - Cloned objects get names like `/std/room#2`, all sharing the same program
   - Objects can be moved to create spatial hierarchies

4. **Comm** ([src/comm.c](src/comm.c)): Non-blocking network I/O
   - Handles telnet connections and dispatches user commands to interactive objects

5. **LPC Compiler** ([lib/lpc/](lib/lpc/)): On-demand compilation via Bison grammar
   - Compiles LPC source to opcodes when objects are loaded
   - Multiple program versions can coexist (key to online editing workflow)

6. **Efuns** ([lib/efuns/](lib/efuns/)): Built-in functions callable from LPC
   - Generated from [func_spec.c](lib/efuns/func_spec.c) via custom preprocessor tool [edit_source](lib/efuns/edit_source.c)

## Critical Developer Workflows

### Building
```bash
# Configure (Linux with Ninja Multi-Config)
cmake --preset linux

# Build (outputs to out/build/linux/src/RelWithDebInfo/neolith)
cmake --build --preset ci-linux
```

CMake presets in [CMakePresets.json](CMakePresets.json):
- `linux`: Linux/WSL with GCC
- `vs16-x64`/`vs16-win32`: Windows Visual Studio 2019
- Presets prefixed `dev-`: incremental builds
- Presets prefixed `ci-`: clean rebuilds

### Testing
Uses GoogleTest framework. Test structure mirrors driver components:
```bash
ctest --preset ut-linux  # Run all tests
```

Test patterns:
- Generic library tests (e.g., [test_logger](tests/test_logger/)) link only needed dependencies
- Driver component tests link the `stem` object library (all driver code except main.c)
- Tests expect [examples/](examples/) directory copied to build directory (automated via CMake POST_BUILD)
- Always set `DISCOVERY_TIMEOUT 20` to avoid cloud antivirus conflicts

### Running
```bash
neolith -f /path/to/neolith.conf
```
Config template: [src/neolith.conf](src/neolith.conf). Set `DebugLogFile` and `LogWithDate` for ISO-8601 timestamped logs.

## Code Conventions & Patterns

### Naming & Style
- **C code**: `snake_case` for functions/variables. No abbreviations; choose unique global names.
- **C++ code**: `CamelCase` for classes/methods. No underscores in test names (GoogleTest restriction).
- **Indentation**: GNU style modified—opening brace on same line as function name for grep-ability.
  ```c
  int my_function(int arg) {  // NOT on new line
      // spaces only, no tabs
  }
  ```
- Return type and function name on same line for searchability.

### Header Includes
All [src/](src/) files include [std.h](src/std.h) first (after config.h), which provides portable POSIX headers and driver-wide options from [efuns/options.h](lib/efuns/options.h).

### Modular Architecture
Legacy code had excessive global variables. Neolith refactors into:
- Static libraries in [lib/](lib/): `port`, `logger`, `rc`, `misc`, `efuns`, `lpc`, `socket`
- `stem` object library in [src/](src/): all driver code linked to tests and main executable
- Cascaded `init_*()`/`deinit_*()` lifecycle, tracked via `get_machine_state()`

### Apply Functions Pattern
"Applies" are LPC functions called by the driver (inverse of efuns). Key examples:
- `heart_beat()`: called every HEARTBEAT_INTERVAL (default 2s) if object calls `set_heart_beat(1)`
- `reset()`: lazy initialization when object accessed after timeout
- `init()`: called when objects move in inventory hierarchy
- `receive_message()`: delivers messages from `tell_object()`, `say()`, etc.

Applies are called via [apply_low()](src/apply.c) with caching for performance. See [docs/applies/](docs/applies/).

### Object Lifecycle
- **Loading**: Compile LPC source → creates initial object named without `#`
- **Cloning**: `clone_object()` creates instance with `#N` suffix, shares program
- **Versioning**: Multiple programs can coexist; cloned objects keep their program even after source recompile
- **Destruction**: `destruct()` decrements program refcount; program freed when count hits zero

### CMake Library Dependencies
```
stem (driver) → efuns, lpc, rc, socket, misc, logger, port
    lpc → logger, efuns, rc
    efuns → port, misc
```
When adding features, link appropriate libraries. Check [src/CMakeLists.txt](src/CMakeLists.txt) for the dependency chain.

### Efuns Code Generation
Efuns are **not** manually registered. Instead:
1. Define function spec in [func_spec.c](lib/efuns/func_spec.c)
2. Build system runs C preprocessor → `func_spec.i`
3. Custom tool `edit_source` generates tables consumed by compiler
4. Add C implementation in [lib/efuns/](lib/efuns/) with `#ifdef F_FUNCTION_NAME` guards

## Platform-Specific Patterns

### Portability Layer ([lib/port/](lib/port/))
- Windows requires custom implementations: `crypt()`, `getopt()`, `gettimeofday()`, `realpath()`, `symlink()`
- Timer backends selected at configure time: Win32 timer / POSIX `timer_create()` / fallback `pthread` timer
- CMake generator expressions handle platform differences (see [lib/port/CMakeLists.txt](lib/port/CMakeLists.txt))

### Compiler Differences
- MSVC: `/W4 /wd4706 /permissive- /Zc:__cplusplus /utf-8`, defines `_CRT_SECURE_NO_WARNINGS`
- GCC/Clang: `-Wall -Wextra -Wpedantic`, defines `_GNU_SOURCE`

## Documentation Conventions
- Markdown in [docs/](docs/) using GitHub Flavored Markdown
- Inline doxygen comments for functions:
  ```c
  /**
   * @brief Brief description.
   * @param arg Description of parameter.
   * @returns What the function returns.
   */
  ```
- Efun docs: [docs/efuns/](docs/efuns/) in Markdown
- Apply docs: [docs/applies/](docs/applies/) in Markdown
- **Agent-generated implementation reports**: Save to [docs/history/agent-reports/](docs/history/agent-reports/)
  - Name pattern: `feature-name-phaseN.md` or `feature-name-YYYY-MM-DD.md`
  - Include: summary, components delivered, test results, next steps, files modified

## Key File Locations
- **Build config**: [config.h.in](config.h.in) → `out/build/*/config.h` (feature detection results)
- **Runtime config template**: [src/neolith.conf](src/neolith.conf)
- **Test mudlib**: [examples/m3_mudlib/](examples/m3_mudlib/)
- **Developer reference**: [docs/manual/dev.md](docs/manual/dev.md)
- **Internals guide**: [docs/manual/internals.md](docs/manual/internals.md)

## Common Pitfalls
1. **Don't modify generated files** like `grammar.c`/`grammar.h` (from Bison) or efun tables (from edit_source)
2. **Object destruction**: Always check `ob->flags & O_DESTRUCTED` after applies—objects can self-destruct
3. **Stack discipline**: Applies must clean up arguments even on failure (see [apply.c](src/apply.c) comments)
4. **Global state**: Minimize globals; use `static` within .c files when possible
5. **Line-of-code metrics**: Avoid unnecessary line wrapping; check LOC with `git ls-files | egrep -v '^(docs|examples)' | xargs wc -l`

## When Contributing
- Add unit tests in [tests/](tests/) subdirectories following GoogleTest patterns
- Document new efuns in [docs/efuns/](docs/efuns/)
- Update [docs/ChangeLog.md](docs/ChangeLog.md)
- See [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) for full guidelines
