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
- CMake build presets should be used from top-level directory

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
- CTest presets should be used from top-level directory

#### LPC Object Loading in Tests
When testing LPC compilation/object loading, use `load_object()` with the `pre_text` parameter:

```cpp
// Initialize required subsystems first
setup_simulate();
init_simul_efun(CONFIG_STR(__SIMUL_EFUN_FILE__));
init_master(CONFIG_STR(__MASTER_FILE__));
current_object = master_ob;

// Load from inline LPC code (no filesystem required)
object_t* obj = load_object("test_obj.c", "void create() { }\n");
ASSERT_NE(obj, nullptr);
EXPECT_STREQ(obj->name, "test_obj");

// Cleanup
destruct_object(obj);
tear_down_simulate();
```

The `pre_text` parameter (Neolith extension) allows compiling LPC source inline without creating physical files, streamlining unit tests.

#### Socket Testing Patterns
Tests using network sockets require special initialization handling:

**Header Includes**:
```cpp
#include <gtest/gtest.h>
extern "C" {
#include "port/io_reactor.h"
#include "port/socket_comm.h"  // Provides create_test_socket_pair()
}
```

**Windows Socket Initialization**:
- Use global `WinsockEnvironment` class in main test file (see [test_io_reactor_main.cpp](tests/test_io_reactor/test_io_reactor_main.cpp))
- DO NOT call `socket_comm_init()`/`socket_comm_cleanup()` in individual tests
- Global environment handles `WSAStartup()`/`WSACleanup()` for entire test run
- Pattern:
```cpp
#ifdef WINSOCK
class WinsockEnvironment : public ::testing::Environment {
    // SetUp() calls WSAStartup(), TearDown() calls WSACleanup()
};
static ::testing::Environment* const winsock_env =
    ::testing::AddGlobalTestEnvironment(new WinsockEnvironment);
#endif
```

**Test File Organization**:
- Main test file (e.g., `test_io_reactor_main.cpp`): Contains WinsockEnvironment and GoogleTest main()
- Individual test files (e.g., `test_io_reactor_console.cpp`): Include common header, write tests
- Common header (e.g., `test_io_reactor_common.h`): Shared includes and helper functions

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

### Integer Type Usage
- **LPC runtime integers**: Always `int64_t` (svalue_u.number is int64_t)
- **Format strings**: Use `PRId64` macro for printing int64_t values
- **Never use `long`**: Platform-specific size (32-bit on Windows x64, 64-bit on Linux)
- **Small integers**: `int` is fine for loop counters, array indices, etc.
- **push_number()**: Accepts `int64_t`, automatically handles conversion

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

## LPC Type Systems

Neolith uses **three distinct type systems** that must never be mixed:

1. **Compile-time types** (`lpc_type_t`): TYPE_* constants (0-10) for static type checking
2. **Runtime types** (`svalue_type_t`): T_* bit flags for runtime value dispatch
3. **Parse tree types** (`lpc_type_t`): TYPE_* annotations in AST nodes

**Critical Rules**:
- `lpc_type_t` uses hybrid encoding: sequential base (0-10) + bit flag modifiers (TYPE_MOD_ARRAY=0x0020, TYPE_MOD_CLASS=0x0040, NAME_TYPE_MOD=0x7F00)
- `svalue_type_t` uses pure bit flags: T_NUMBER=0x2, T_STRING=0x4, T_ARRAY=0x8, etc.
- **Never use TYPE_* with svalue_t or T_* with lpc_type_t**—they are incompatible domains
- Always mask NAME_TYPE_MOD (0x7F00) when checking base types: `type & ~NAME_TYPE_MOD`
- Check arrays with: `if (type & TYPE_MOD_ARRAY)`
- Check classes with: `IS_CLASS(type)` macro

**Structures using `lpc_type_t`**: compiler_function_t.type, variable_t.type, class_member_entry_t.type, parse_node_t.type

See [docs/internals/lpc-types.md](docs/internals/lpc-types.md) for complete type system reference.

## LPC Compiler Architecture

The LPC compiler uses a multi-pass architecture with 24 memory blocks (`mem_block[]`) for incremental program construction. See [docs/internals/lpc-program.md](docs/internals/lpc-program.md) for complete details.

### Memory Block System

**15 Permanent Areas** (saved in final `program_t`):
- **A_PROGRAM** (0): Bytecode instructions (max 64KB)
- **A_RUNTIME_FUNCTIONS** (1): Function dispatch table (`runtime_function_u[]`)
- **A_COMPILER_FUNCTIONS** (2): Function definitions defined at this level (`compiler_function_t[]`)
- **A_RUNTIME_COMPRESSED** (3): Compressed function table for deep inheritance
- **A_FUNCTION_FLAGS** (4): Function modifiers (static, private, inherited, etc.)
- **A_STRINGS** (5): String literal table (shared strings)
- **A_VAR_NAME/TYPE** (6-7): Variable names and types
- **A_LINENUMBERS** (8): Debug info mapping bytecode→source lines
- **A_FILE_INFO** (9): Include file boundaries
- **A_INHERITS** (10): Inherited program references
- **A_CLASS_DEF/MEMBER** (11-12): LPC class definitions
- **A_ARGUMENT_TYPES/INDEX** (13-14): Function argument types (if `#pragma save_types`)

**9 Temporary Areas** (compilation only):
- **A_CASES** (15): Switch case tracking
- **A_STRING_NEXT/REFS** (16-17): String hash table bookkeeping
- **A_INCLUDES** (18): Include file list for binary validation
- **A_PATCH** (19): String switch bytecode offsets for patching
- **A_INITIALIZER** (20): Global variable initialization code (becomes `__INIT()`)
- **A_FUNCTIONALS** (21): Unused/vestigial
- **A_FUNCTION_DEFS** (22): Function inheritance/alias tracking (`compiler_temp_t[]`)
- **A_VAR_TEMP** (23): All variables including inherited

**Key Macros** (defined in [lib/lpc/compiler.h](lib/lpc/compiler.h)):
```c
COMPILER_FUNC(n)    // Get compiler_function_t from A_COMPILER_FUNCTIONS
FUNCTION_RENTRY(n)  // Get runtime_function_u from A_RUNTIME_FUNCTIONS
FUNCTION_FLAGS(n)   // Get flags from A_FUNCTION_FLAGS
FUNCTION_TEMP(n)    // Get compiler_temp_t from A_FUNCTION_DEFS
INHERIT(n)          // Get inherit_t from A_INHERITS
VAR_TEMP(n)         // Get variable_t from A_VAR_TEMP
PROG_STRING(n)      // Get string from A_STRINGS
CLASS(n)            // Get class_def_t from A_CLASS_DEF
```

### Compilation Lifecycle

1. **prolog()**: Allocate 24 blocks at 4KB each (grows via doubling)
2. **Parsing**: Bison grammar in [lib/lpc/grammar.y](lib/lpc/grammar.y) builds parse trees
3. **Code Generation**: [lib/lpc/program/icode.c](lib/lpc/program/icode.c) walks trees, emits bytecode
4. **epilog()**: 
   - Resolve undefined/inherited functions
   - Sort function table alphabetically
   - Copy 15 permanent areas into single contiguous `program_t`
   - Free temporary blocks
5. **clean_parser()**: On errors, free all blocks and shared strings

**Block Switching**: Compiler switches between `A_PROGRAM` (main code) and `A_INITIALIZER` (variable init) via `switch_to_block()`. The `__INIT()` function is appended at end of compilation if A_INITIALIZER is non-empty.

### Binary Serialization

Neolith can save compiled programs to `.b` files (enabled via `#pragma save_binary` and `__SAVE_BINARIES_DIR__` config). This avoids re-parsing on subsequent driver starts. See [docs/internals/lpc-program.md#binary-serialization](docs/internals/lpc-program.md#binary-serialization).

**Binary Format**:
```
[Magic:"NEOL"][Driver ID][Config ID (simul_efun mtime)]
[Include list][Program name][program_t with relative pointers]
[Inherited names][String table][Variable names][Function names]
[Line numbers][Patches for string switches]
```

**Pointer Conversion**:
- **Save** ([locate_out()](lib/lpc/program/binaries.c)): Convert absolute pointers → offsets from `program_t` base
- **Load** ([locate_in()](lib/lpc/program/binaries.c)): Convert offsets → absolute pointers
- **String Switches** ([patch_out/in()](lib/lpc/program/binaries.c)): Convert embedded string pointers ↔ table indices, then re-sort

**Validation**: Binary rejected if source/includes newer, driver changed, or simul_efun changed. Falls back to full compilation.

**Critical for Binary Save/Load**:
- All shared strings must be recreated via `make_shared_string()`
- Function table must be re-sorted (pointer addresses change between runs)
- Inherited programs must be loaded first (triggers recursive loads)
- Switch tables need special patching (tracked in `A_PATCH` during compilation)

### Function Dispatch

**Three Index Types**:
- `function_number_t`: Index into `A_COMPILER_FUNCTIONS` (only defined functions)
- `function_index_t`: Index into `A_RUNTIME_FUNCTIONS` (all functions including inherited)
- `function_address_t`: Bytecode offset in `A_PROGRAM`
- When passing index types to functions, use `int` for compatibility. When returning, use specific typedefs. When storing in structs, use specific typedefs for clarity.

**Inheritance Resolution**:
- Local functions: `runtime_function_u.def` has `{num_arg, num_local, f_index}`
- Inherited: `runtime_function_u.inh` has `{inherit_offset, parent_function_index}`
- Flag `NAME_INHERITED` in `A_FUNCTION_FLAGS` distinguishes them
- Offsets in `inherit_t` translate parent indices to child namespace

**Function Lookup**: Binary search on sorted `function_table` by name pointer address (not strcmp). Functions starting with `#` always last.

### Common Compiler Tasks

**Adding New Bytecode Instruction**:
1. Define opcode in [lib/efuns/include/function.h](lib/efuns/include/function.h)
2. Add code generation in [lib/lpc/program/icode.c](lib/lpc/program/icode.c)
3. Add execution in [src/interpret.c](src/interpret.c)
4. Update disassembler in [lib/lpc/program/disassemble.c](lib/lpc/program/disassemble.c)

**Adding Grammar Production**:
1. Update [lib/lpc/grammar.y](lib/lpc/grammar.y)
2. Build parse tree node in [lib/lpc/program/parse_trees.c](lib/lpc/program/parse_trees.c)
3. Add code generation case in [lib/lpc/program/icode.c](lib/lpc/program/icode.c)

**Debugging Compilation**:
- Set `TT_COMPILE` trace flags in config for verbose logging
- Use `opt_trace(TT_COMPILE|level, ...)` throughout compiler
- Check `num_parse_error` counter
- Inspect `mem_block[].current_size` to see area growth

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
- **Agent-generated analysis and implementation reports**: Save to [docs/history/agent-reports/](docs/history/agent-reports/)
  - Name pattern: `feature-name-phaseN.md` or `feature-name-YYYY-MM-DD.md`
  - Include: summary, components delivered, test results, next steps, files modified
  - Following the same style as implementation details in [docs/internals/](docs/internals/) but don't link to source files directly since they may be outdated

### Documentation Best Practices
1. **Don't duplicate implemented code in documentation**
   - Once code is committed and tested, reference it with links instead of copying it
   - Example: "See [io_reactor.h](lib/port/io_reactor.h)" not full API declarations
2. **Explain committed code concisely**
   - Document WHY decisions were made, not HOW the code works (code shows that)
   - Focus on design rationale, platform differences, integration patterns
   - Keep implementation reports focused on what changed and why it matters
3. **Remove redundant content**
   - Delete verbose examples when linking to actual source is sufficient
   - Avoid repeating test output verbatim—summarize pass/fail status instead
   - Don't show complete function implementations that are already in source files
4. **Keep design documents concise**
   - Main design docs should be reference guides, not implementation tutorials
   - Move detailed examples to phase-specific reports or source code comments
   - Use tables and bullet points over lengthy prose

## Key File Locations
- **Build config**: [config.h.in](config.h.in) → `out/build/*/config.h` (feature detection results)
- **Runtime config template**: [src/neolith.conf](src/neolith.conf)
- **Test mudlib**: [examples/m3_mudlib/](examples/m3_mudlib/)
- **Developer reference**: [docs/manual/dev.md](docs/manual/dev.md)
- **Internals guide**: [docs/manual/internals.md](docs/manual/internals.md)
- **LPC type systems**: [docs/internals/lpc-types.md](docs/internals/lpc-types.md) - compile-time vs runtime types, encoding schemes
- **LPC compiler internals**: [docs/internals/lpc-program.md](docs/internals/lpc-program.md) - mem_block system and binary serialization
- **Design ideas, draft and plans**: [docs/plan/](docs/plan/) - any design docs, proposals, feature plans before implementation

## Reference Documentation

Documentation files should be named using lowercase letters and dashes (`-`) as word separators. Avoid underscores and camelCase.
Prefix the filenames with the library name, feature or subsystem name for easy identification (e.g., `async-dns-integration.md`).

**Design & Planning** ([docs/plan/](docs/plan/)):
- Create design docs here before implementation starts. Move to manual/internals when implementation begins.
- When extending an existing feature, update the original design doc instead of creating a new one.
- Don't duplicate implementation details here; focus on high-level design, rationale, alternatives considered, and final decisions.

**High-level Design Documentation** ([docs/manual/](docs/manual/)):
- [admin.md](docs/manual/admin.md): Admin guide for server operators - configuration options, logging, performance tuning.
- [lpc.md](docs/manual/lpc.md): LPC language reference - syntax, semantics, standard libraries.
- [efuns.md](docs/manual/efuns.md): Comprehensive efun reference manual, categorized by functionality.
- [dev.md](docs/manual/dev.md): Developer workflow and build system, testing patterns and git workflow.
- [unit-tests.md](docs/manual/unit-tests.md): Guidelines for writing and organizing unit tests using GoogleTest.
- [console-mode.md](docs/manual/console-mode.md): Using the interactive console mode for debugging and live interaction.
- [internals.md](docs/manual/internals.md): Driver architecture overview. Links to [docs/internals/](docs/internals/) for deep dives into specific subsystems.
- [trace.md](docs/manual/trace.md): Debugging and tracing guide - how to enable and interpret trace logs.
- When extending an existing feature, update the original design doc instead of creating a new one.
- Keep these documents updated as the codebase evolves. Focus on high-level architecture and terminology for current code.
- High-level concepts includes features that are visible to mudlib developers (efuns, applies, object model, compiler behavior).
- When starting new features, create design docs in [docs/plan/](docs/plan/) first, then move to manual when implementation starts. Keep implementation status updated. Link implementation details back to design docs.

**Implementation Details** ([docs/internals/](docs/internals/)):
- Keep these documents focused on design decisions, technical specifications and internal architecture such as C APIs and data structures.
- Update as implementation details change. Link back to high-level design docs in [docs/manual/internals.md](docs/manual/internals.md).
- [lpc-types.md](docs/internals/lpc-types.md): Complete LPC type system reference - lpc_type_t vs svalue_type_t, encoding schemes, compatibility checking, common pitfalls
- [lpc-program.md](docs/internals/lpc-program.md): Complete LPC compiler memory block system, binary save/load format, pointer serialization, inheritance resolution
- [int64-design.md](docs/internals/int64-design.md): Platform-agnostic 64-bit integer implementation - runtime types, bytecode encoding, binary compatibility
- [async-library.md](docs/internals/async-library.md): Async library design - queues, workers, runtime integration

When working on compiler features, consult these documents for:
- **Type system rules**: lpc_type_t vs svalue_type_t domains, masking NAME_TYPE_MOD, array/class detection
- **Integer handling**: svalue_u.number is int64_t, use PRId64 for formatting, F_LONG opcode for large literals
- Memory block allocations and their data types
- Binary file format and version validation
- Function/variable/class indexing schemes
- Pointer conversion during serialization
- Switch table patching mechanics

## Common Pitfalls
1. **Don't modify generated files** like `grammar.c`/`grammar.h` (from Bison) or efun tables (from edit_source)
2. **Object destruction**: Always check `ob->flags & O_DESTRUCTED` after applies—objects can self-destruct
3. **Stack discipline**: Applies must clean up arguments even on failure (see [apply.c](src/apply.c) comments)
4. **Global state**: Minimize globals; use `static` within .c files when possible
5. **Line-of-code metrics**: Avoid unnecessary line wrapping; check LOC with `git ls-files | egrep -v '^(docs|examples)' | xargs wc -l`
6. **Type system mixing**: Never mix compile-time TYPE_* with runtime T_* values—see [lpc-types.md](docs/internals/lpc-types.md)
7. **Binary compatibility**: Always bump driver_id in [binaries.c](lib/lpc/program/binaries.c) when adding/removing/reordering opcodes or changing runtime struct sizes

## When Contributing
- Add unit tests in [tests/](tests/) subdirectories following GoogleTest patterns
- Document new efuns in [docs/efuns/](docs/efuns/)
- Update [docs/ChangeLog.md](docs/ChangeLog.md)
- See [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) for full guidelines
