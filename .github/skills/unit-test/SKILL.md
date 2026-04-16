---
name: unit-test
description: "Add unit tests for individual components. Use the GoogleTest (GTest) framework to define test cases and assertions. Troubleshooting test failures involves checking test logs, reproducing failures locally, and debugging with breakpoints or additional logging to identify root causes."
---
# Unit Test Skill Instructions
Unit testing involves writing test cases for individual components of the codebase to verify their correctness in isolation. This helps catch bugs early and ensures that each part of the system behaves as expected.
## Writing Unit Tests

### CMake settings for unit test programs
- In `CMakeLists.txt`, link unit-test program to `GTest::gtest_main` and the specific libraries needed for the test (e.g., `async`, `socket`, `stem` for driver tests)
Test patterns:
  - Generic library tests (e.g., [test_logger](tests/test_logger/)) link only needed dependencies
  - Driver component tests link the `stem` object library (all driver code except main.c)
- `gtest_discover_tests()` is required for per-case discovery in CTest; manual `add_test()` registration can hide individual cases and produce `_NOT_BUILT`-style confusion.
- For tests that require mudlib access, add a custom command to copy the testing mudlib in [examples/](examples/) to the build directory at POST_BUILD. Pattern:
  ```cmake
  add_custom_command(TARGET test_lpc_compiler POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/m3_mudlib ${CMAKE_CURRENT_BINARY_DIR}/m3_mudlib
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/examples/m3.conf ${CMAKE_CURRENT_BINARY_DIR}
  )
  ```
- Always set `DISCOVERY_TIMEOUT 20` to avoid cloud antivirus conflicts

### Unit Test Fixtures

- When adding unit tests, reuse existing fixture classes defined in `fixtures.hpp` when possible. These provide common setup/teardown for subsystems like simulate, async runtime, etc.
- Use `get_machine_state()` to check subsystem initialization state and avoid redundant setup. For example, if testing a feature that requires simulate, check if it's already initialized before calling `setup_simulate()`.
- The `SetUp()` method in the test fixture should follow the same order as in `src/main.c`: configurations, then resource pools, LPC compiler, followed by world simulation and mudlib vital objects and epilogue. This ensures that all dependencies are properly initialized for the tests.
- The `TearDown()` method should clean up resources in the reverse order of setup to ensure proper teardown without leaving dangling pointers or unfreed memory. See `do_shutdown()` in `src/simulate.c` for reference.
- Use the `Test` suffix for fixture classes (e.g. `LPCCompilerTest`) to follow GoogleTest conventions.
- **Locating `m3.conf` in fixture SetUp()**: Before calling `init_stem()` to setup MAIN_OPTIONS, locate `m3.conf` from the current working directory first (${CMAKE_CURRENT_BINARY_DIR}, hopefully).
  - If not found, fallback to the parent directory of current working directory (to support running tests from the test executable's own directory).
  - This happens when IDE or other tools have no knowledge of the `WORKING_DIRECTORY` set in CMake. Pattern:
```cpp
    namespace fs = std::filesystem;
    fs::path config_dir = fs::current_path();
    if (!fs::exists(config_dir / "m3.conf"))
      config_dir = fs::current_path().parent_path();
    init_stem(3, (unsigned long)-1, (config_dir / "m3.conf").string().c_str());
```
- `init_stem()` is used for creating a driver-like environment similar to the command line interface of the MUD driver.
  - Use `MAIN_COPTION` macro to set/get any driver options needed for the test.
  - The order of applying options matters. Rule of thumb: call `init_config(MAIN_OPTION(config_file))` after `init_stem()` to apply options from config file, then apply any test-specific overrides with `MAIN_COPTION` after that.
  - For unit-test environment, `pedantic` mode is recommended since it enables stricter checks and cleans up all objects in `tear_down_simulate()`, saving the code to call `destruct_object()` manually in each test and preventing state leakage between tests.
- After `m3.conf` is loaded, locate the mudlib directory from the config and change wokring directory to the mudlib directory. Note that if the mudlib directory is not an absolute path, it's relative to the directory of `m3.conf`. Pattern:
```cpp
    fs::path mudlib_path = fs::path(CONFIG_STR(__MUD_LIB_DIR__));
    if (mudlib_path.is_relative())
      mudlib_path = config_dir / mudlib_path;
    s_previous_cwd = fs::current_path(); // saved for later restoration in TearDown()
    fs::current_path(mudlib_path);
```
### LPC Object Loading in Tests
Make sure `init_master()` is called in the fixture setup before loading LPC objects, as the master object is responsible for approving `load_object()` calls. Set `current_object` to `master_ob` when loading objects in tests to ensure the master applies to approve the load.

- Load `simul_efun` only when the test explicitly needs simul efuns. Avoid eager `init_simul_efun()` in generic compiler/load tests because inherited objects from simul_efun dependencies (for example, `/api/unicode`) can remain allocated and appear as leaks at teardown.

When testing LPC compilation/object loading, use `load_object()` with the second parameter `pre_text` to load LPC source code directly from a string. This avoids the need for filesystem access and allows for more self-contained unit tests. Pattern:

```cpp
// Use a fixture that sets up simulate and master object
// simul_efun is optional depending on test needs

// Load from inline LPC code (no filesystem required)
current_object = master_ob; // Set current_object for master applies to approve the load_object() call
object_t* obj = load_object("test_obj.c", "void create() { }\n");
ASSERT_NE(obj, nullptr);
EXPECT_STREQ(obj->name, "test_obj");

// Cleanup
destruct_object(obj);
```

### Lessons Learned: Testing Master Applies with pre_text

- For tests that need custom master callbacks (for example, `error_handler`), inject them through `init_master(master_file, pre_text)` instead of editing mudlib files.
- Keep assertions contract-focused: verify the injected master function is present/compilable, then trigger the driver path that must route through that apply.
- Prefer behavior checks over source-lock checks. Do not assert raw source snippets from repository files, since those tests are brittle and block refactoring.
- Use `pre_text` instrumentation as test-only setup and keep production paths unchanged by passing `NULL` where no injection is needed.
- For error-path coverage, assert routing outcomes (for example, error propagation through the master callback path) rather than relying on fragile side-effect counters.

### Calling LPC functions from tests
1. Get the shared string for the function name using `findstring` (e.g., `findstring("create", NULL)`)
2. Call `find_function()` to get the function index from the program and the `program_t` pointer if the function is inherited from a parent program. Note that `find_function` requires a `shared_str_t` returned from `findstring`. Pattern:
```cpp
// found_prog is the program where the function is defined.
// index is the COMPILER function index (position in found_prog's function_table).
// function index offset for inherited functions is returned via fio.
// variable index offset for inherited globals is returned via vio.
program_t* found_prog = find_function(prog, findstring("add"), &index, &fio, &vio);
ASSERT_NE(found_prog, nullptr);
```
3. If `found_prog` is different from `prog`, the function is inherited. The `index` can be used to obtain the `compiler_function_t` in `found_prog`'s function_table where function address and argument count are stored. **CRITICAL:** `index` is a compiler index, NOT a runtime index. Convert it using `found_prog->function_table[index].runtime_index` then add `fio` for inheritance offset: `(found_prog->function_table[index].runtime_index + fio)` gives the runtime dispatch index in `prog`.
4. Push arguments onto the stack using `push_number()`, typed string push helpers, and related APIs in [src/stack.c](src/stack.c).
  - `push_shared_string(shared_str_t)` is for shared-string ownership paths.
  - `push_malloced_string(malloc_str_t)` is for malloc-string ownership paths.
  - Avoid passing raw `char *` directly to these typed push helpers in new tests.
5. Call `call_function()` with the actual function runtime index and number of arguments already pushed onto the stack (`num_args`). Pattern:
```cpp
// Convert compiler index to runtime index, then add inheritance offset
int runtime_index = found_prog->function_table[index].runtime_index + fio;
// For void functions, ret can be a dummy svalue_t since it won't be used
svalue_t ret;
call_function (prog, runtime_index, num_args, &ret);
```

### Typed String Boundary Lessons Learned

- In tests that validate subtype-specific behavior, use the typed push helpers (`push_shared_string(shared_str_t)`, `push_malloced_string(malloc_str_t)`) rather than generic string push paths so subtype bugs fail at the right boundary.
- Create any function-name lookup key through `findstring(...)` and keep it as `shared_str_t`; do not downcast to plain `char *` in test code.
- When a test asserts string subtype handling, verify both data and ownership side effects (for example, no premature free/unlink after push/pop flow).

### LPC Callback Testing Lessons (from socket behavior tests)

- Prefer callback-owner LPC objects loaded from inline `pre_text` source instead of C-only mocks when asserting callback ordering/content. This exercises real apply dispatch and ownership checks.
- Record callback events in the LPC object (for example, append `{name, fd, payload}` tuples) and expose query/clear helpers so tests can assert exact sequences across multiple phases.
- For flows that cross security boundaries (for example, release/acquire handoff), add narrow internal test seams at the C layer instead of relaxing production security policy. Keep seams opt-in and test-only.
- Drive readiness-dependent callbacks deterministically by using loopback sockets plus explicit poll/flush steps in fixtures. Avoid timing-based sleeps whenever possible.
- Keep return-code assertions platform-aware for nonblocking operations. Where behavior is intentionally dual-path (`EESUCCESS` vs callback completion), assert allowed set membership and then assert callback side effects.
- Clear callback event buffers between action phases (connect, write, close, release) so each assertion targets one transition and failures are easier to localize.
- In callback-path tests, always verify terminal invariants: no duplicate terminal callback, no callback after close, and cleanup still succeeds if objects self-destruct during applies.

### Reusable Checklist: LPC Callback Tests (Pre-Merge)

Use this checklist before merging callback-path tests:

- [ ] Callback owner is a real LPC object loaded via `pre_text` (not only C-side mocks).
- [ ] Test records callback events in LPC and exposes query/clear helpers.
- [ ] Event buffers are cleared between phases so each assertion covers one transition.
- [ ] Nonblocking return-code assertions allow valid platform-dependent outcomes where required.
- [ ] Callback order is asserted explicitly for the scenario under test.
- [ ] Terminal invariants are asserted: no duplicate terminal callback, no callbacks after close.
- [ ] Security-boundary behavior is tested without weakening production policy.
- [ ] Any seam used is narrow, opt-in, and test-only.
- [ ] Test avoids sleep-based timing; readiness/progress is driven deterministically.
- [ ] Test teardown verifies cleanup succeeds even after callback execution paths.

### Socket Testing Patterns

Tests using network sockets require special initialization handling:

**Header Includes**:
```cpp
#include <gtest/gtest.h>
#include "async/async_runtime.h"
#include "port/socket_comm.h"  // Provides create_test_socket_pair()
```

**Windows Socket Initialization**:
- Include the `port/socket_comm.h` for configured socket header inclusion and utility functions.
- Make sure a static instance of `testing::Environment` that calls `WSAStartup()`/`WSACleanup()` is created when `WINSOCK` is defined. Pattern:
```cpp
#ifdef WINSOCK
// In the test implementation file, define a testing environment to perform process-wide WINSOCK initialization and cleanup for all socket tests.
class WinsockEnvironment : public ::testing::Environment {
    // SetUp() calls WSAStartup(), TearDown() calls WSACleanup()
};
static ::testing::Environment* const winsock_env =
    ::testing::AddGlobalTestEnvironment(new WinsockEnvironment);
#endif
```
- **IMPORTANT**: For Winsock, only call `AddGlobalTestEnvironment` once per test binary.

### RAII and Fixture Ownership**:
- Put shared subsystem lifecycle in the test fixture when every test in the binary depends on it. Example: initialize and tear down `async_runtime` in `SetUp()` / `TearDown()` rather than in per-test guards.
- Use small RAII guards only for optional or test-local state, such as temporary hooks, resolver initialization, or object-context switching.
- Each RAII guard should own one boundary only. Avoid composite guards that mix unrelated responsibilities like runtime creation plus resolver initialization.
- Prefer shared helper guards in a common `fixtures.hpp` over duplicating identical RAII classes across multiple test `.cpp` files.
- When isolating tests that use shared global state, a dedicated RAII guard may reset just that subsystem per test while the fixture continues to own the broader runtime.

## Unit Test File Organization

- The testing mudlib is copied to ${CMAKE_CURRENT_BINARY_DIR} so the unit-test programs of different configurations can be launched from this directory and locate the mudlib config file `m3.conf` from current working directory. Pattern:
```bash
cd out/build/linux/tests/test_lpc_compiler
./RelWithDebInfo/test_lpc_compiler
```

## Type-Safe svalue_t Assertions with svalue_view

**Preferred Pattern for Test Assertions**

When asserting results on the eval stack or validating `svalue_t` values, use the `lpc::svalue_view` non-owning wrapper instead of direct union-member access. This provides type-safe accessors that eliminate common mistakes and improve test code clarity.

**Include the wrapper header:**
```cpp
#include "lpc/types.h"  // includes the svalue_view wrapper in C++ context
```

**Replace raw union access with view accessors:**

*Before (unsafe, prone to type confusion):*
```cpp
ASSERT_EQ(sp->type, T_STRING);
EXPECT_STREQ(SVALUE_STRPTR(sp), "hello");
```

*After (type-safe, clearer intent):*
```cpp
auto view = lpc::svalue_view::from(sp);
ASSERT_TRUE(view.is_string());
EXPECT_STREQ(view.c_str(), "hello");
```

**String accessors (by subtype for clarity):**
- `view.c_str()` — Safe const pointer (null-terminated for all subtypes)
- `view.shared_string()` — Retrieve shared-string subtype (or nullptr)
- `view.malloc_string()` — Retrieve malloc-string subtype (or nullptr)
- `view.const_string()` — Retrieve constant-string subtype (or nullptr)

**Numeric/object accessors:**
- `view.is_number()` — Type predicate
- `view.number()` — Retrieve int64_t value (0 if not T_NUMBER)
- `view.is_object()` — Type predicate
- `view.object()` — Retrieve object_t* (nullptr if not T_OBJECT)

**Type predicates (for selective assertions):**
- `view.is_string()`, `view.is_counted()`, `view.is_shared()`, `view.is_malloc()`, `view.is_constant()`

**Null-safety**
The wrapper safely handles null `svalue_t*` pointers. All accessors check the pointer and return safe defaults (nullptr, 0, false) if the pointer is null.

**Set up stack values using the wrapper (for consistency):**
```cpp
svalue_t sv;
auto view = lpc::svalue_view::from(&sv);
view.set_malloc_string(allocated_str);  // Set type and union atomically
view.set_shared_string(shared_str);
view.set_number(42);
```

**Key benefits:**
- Eliminates raw `sp->u.X` access that bypasses type checks
- Makes intended assertions clearer (read `.c_str()` not raw union members)
- Scales across string subtypes without duplicating test logic

This wrapper pattern is now **the preferred form for all new test code that interacts with svalue_t structures**.
