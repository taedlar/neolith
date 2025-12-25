# Neolith LPMud Driver - AI Agent Instructions

## Project Overview
Neolith is a minimalist LPMud (Multi-User Dungeon) game server forked from MudOS v22pre5. It's a **C/C++ virtual machine** that executes LPC (Lars Pensjö C) scripting language programs to run text-based multiplayer games. The architecture follows a classic interpreter pattern with a compiler, stack machine, and runtime.

## Architecture Components

### Core Runtime Layers (see [src/README.md](../src/README.md))
- **Backend** ([src/backend.c](../src/backend.c)) - Main I/O loop, handles `heart_beat()`, `reset()` applies, and `call_out()` scheduling
- **Comm** ([src/comm.c](../src/comm.c)) - Non-blocking TCP socket handling, telnet protocol, user command dispatch
- **Interpret** ([src/interpret.c](../src/interpret.c)) - Stack machine interpreter executing LPC bytecode opcodes via `eval_instruction()`
- **Simulate** ([src/simulate.c](../src/simulate.c)) - Object lifecycle (load/clone/destruct), mudlib filesystem mapping, world hierarchy

### LPC Compiler ([lib/lpc/](../lib/lpc/))
- **grammar.y** - Bison parser generating parse trees from LPC source
- **compiler.c** - Translates parse trees to bytecode opcodes
- **lex.c** - Lexer with C preprocessor support
- Programs are reference-counted; multiple objects can share one program image while maintaining separate variable instances

### Efuns (External Functions) ([lib/efuns/](../lib/efuns/))
- Built-in functions callable from LPC, defined with `#ifdef F_FUNCTION_NAME` pattern
- Implementation pattern: `void f_function_name(void)` reading args from global `sp` (stack pointer)
- Example: [lib/efuns/bits.c](../lib/efuns/bits.c) shows `f_test_bit()`, `f_set_bit()` operating on stack

### Object System
Objects = **Program** (shared bytecode) + **Variables** (per-instance). Initial object named `/std/room`, clones are `/std/room#2`, `/std/room#5`. This allows online code updates without disrupting active objects—a key LPMud advantage.

## Build System

### CMake Structure
- **Presets-based workflow**: Use `cmake --preset linux` then `cmake --build --preset ci-linux`
- Executables output to `out/build/<preset>/src/<config>/neolith`
- Build presets:
  - `dev-linux` - Debug incremental build
  - `pr-linux` - RelWithDebInfo incremental (pre-merge validation)
  - `ci-linux` - RelWithDebInfo clean rebuild (nightly builds)

### Platform Considerations
- **Linux primary target** (Ubuntu 20.04+, WSL supported)
- **Windows** via MSVC requires lib/port compatibility layer ([lib/port/](../lib/port/))
- Required deps: `gcc/g++`, `ninja-build`, `bison`
- Auto-generated `config.h` from [config.h.in](../config.h.in) handles platform headers/functions

### Project Structure
- `lib/port/` - Platform abstraction (Windows getopt, realpath, timers)
- `lib/logger/`, `lib/rc/` - Logging and runtime configuration
- `lib/misc/` - Utility libraries
- `lib/socket/` - Socket efuns implementation

## Development Conventions

### Code Style (Inherited from MudOS)
- **Include pattern**: All `src/*.c` files include `"std.h"` first (never use `<>` for it)
- **std.h hierarchy**: Defines platform headers, `options.h`, `port/wrapper.h`, debug macros
- Use `#ifdef HAVE_CONFIG_H` guards at file tops
- Function definitions use K&R-style formatting (pre-C99 legacy)

### Apply Functions
Special LPC callbacks invoked by driver (see [docs/applies/](../docs/applies/)):
- `valid_*()` - Security checks (e.g., file access permissions)
- `init()` - Called when objects move proximity (command registration)
- Cached in [src/apply.c](../src/apply.c) via `APPLY_CACHE_SIZE` hash table for performance

### Testing with GoogleTest
- Tests in [tests/](../tests/) follow `test_<component>/` structure
- Test fixtures initialize with `init_stem()`, `init_strings()` - see [tests/test_stralloc/test_stralloc.cpp](../tests/test_stralloc/test_stralloc.cpp)
- Many tests require setting working directory to example mudlib ([examples/m3_mudlib/](../examples/m3_mudlib/))
- Run via CTest: `ctest --preset ci-linux` or individual test binaries

## Critical Workflows

### Running the Driver
1. Copy [src/neolith.conf](../src/neolith.conf) and customize:
   - `MudlibDir` - Full path to LPC mudlib files
   - `Port` - TCP port with protocol (e.g., `5000:telnet`)
   - `LogDir` - Full path for debug/error logs
2. Launch: `neolith -f /path/to/neolith.conf`
3. Driver loads master object from mudlib to bootstrap world

### Adding New Efuns

Efuns (External Functions) are built-in C functions callable from LPC code. They operate on the global stack pointer (`sp`) and follow a strict pattern:

**Step 1: Implement the C Function**
Create or modify a file in `lib/efuns/` (group related efuns together):

```c
#ifdef F_MY_NEW_EFUN
void f_my_new_efun(void) {
    // Read arguments from stack (sp points to top-most argument)
    // For efun with 2 args: (sp-1) is arg1, (sp) is arg2
    int arg2 = sp->u.number;      // Read second argument
    int arg1 = (sp-1)->u.number;  // Read first argument
    
    // Pop arguments off stack
    sp -= 2;
    
    // Push result onto stack
    push_number(arg1 + arg2);
}
#endif
```

**Key Stack Operations:**
- `sp` - Global stack pointer (points to top of stack)
- `sp->type` - Check argument type (T_NUMBER, T_STRING, T_OBJECT, T_ARRAY, etc.)
- `sp->u.number` / `sp->u.string` / `sp->u.ob` - Access typed values
- `sp--` or `pop_stack()` - Remove value from stack
- `push_number()` / `push_string()` / `push_object()` - Add result to stack
- Type checking: Use `CHECK_TYPES(sp, T_NUMBER, argnum, F_MY_NEW_EFUN)` macro

**Step 2: Declare in func_spec.c**
Add your efun to `lib/func_spec.c` (LPC syntax, processed by `edit_source` at build time):

```c
int my_new_efun(int, int);
```

The syntax follows LPC function prototypes. Return type and parameter types determine:
- Which arguments are required
- Type checking performed by interpreter
- How arguments are pushed/popped

**Step 3: Add Documentation**
Create `docs/efuns/my_new_efun.md`:

```markdown
### NAME
    my_new_efun() - brief description

### SYNOPSIS
    int my_new_efun(int arg1, int arg2);

### DESCRIPTION
    Detailed description of what the function does.

### RETURN VALUE
    Returns the sum of arg1 and arg2.

### EXAMPLES
    int result = my_new_efun(5, 10); // returns 15
```

**Step 4: Rebuild and Test**
```bash
cmake --build --preset ci-linux
# Test in mudlib by calling the efun from LPC code
```

**Common Patterns:**
- **String arguments**: Use `sp->u.string`, remember to `free_string_svalue(sp)` if needed
- **Array arguments**: Access via `sp->u.arr`, iterate with `sp->u.arr->item[i]`
- **Object arguments**: Check `sp->u.ob->flags & O_DESTRUCTED` before use
- **Variable arguments**: Use `st_num_arg` global to get actual argument count
- **Error handling**: Call `error("*Error message")` for runtime errors (aborts execution)

**See Examples:**
- [lib/efuns/bits.c](../lib/efuns/bits.c) - Simple efuns operating on numbers
- [lib/efuns/strings.c](../lib/efuns/strings.c) - String manipulation
- [lib/efuns/arrays.c](../lib/efuns/arrays.c) - Array operations

### Debugging
- Enable trace logging in config: `DebugLogFile debug.log`, `LogWithDate yes`
- Use `opt_trace()` macros (defined via `stem.h`) for conditional debug output
- Stack machine state inspection via `dump_trace()` functions

## Key Files for Understanding
- [README.md](../README.md) - Architecture diagram, build quickstart
- [src/README.md](../src/README.md) - Runtime component mindmap
- [lib/lpc/README.md](../lib/lpc/README.md) - LPC type system, object lifecycle
- [docs/INSTALL.md](../docs/INSTALL.md) - Build environment setup
- [config.h.in](../config.h.in) - Platform capability detection

## Important Gotchas
- **Stack-based execution**: Most efuns/applies manipulate global `sp` stack pointer directly
- **String interning**: Uses [src/stralloc.c](../src/stralloc.c) for shared string storage (`make_shared_string()`)
- **No commercial use**: GPLv2 + Lars Pensjö restriction forbids monetary gain
- **Legacy C codebase**: Pre-dates C99, many modern standards violations being gradually fixed
