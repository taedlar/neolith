---
name: unit-test
description: "Add unit tests for individual components. Use the GoogleTest (GTest) framework to define test cases and assertions."
---
# Unit Test Skill Instructions
Unit testing involves writing test cases for individual components of the codebase to verify their correctness in isolation. This helps catch bugs early and ensures that each part of the system behaves as expected.
## Writing Unit Tests

### Unit Test Fixtures

- When adding unit tests, reuse existing fixture classes defined in `fixtures.hpp` when possible. These provide common setup/teardown for subsystems like simulate, async runtime, etc.
- Use `get_machine_state()` to check subsystem initialization state and avoid redundant setup. For example, if testing a feature that requires simulate, check if it's already initialized before calling `setup_simulate()`.
- The `SetUp()` method in the test fixture should follow the same order as in `src/main.c`: configurations, then resource pools, LPC compiler, followed by world simulation and mudlib vital objects and epilogue. This ensures that all dependencies are properly initialized for the tests.
- The `TearDown()` method should clean up resources in the reverse order of setup to ensure proper teardown without leaving dangling pointers or unfreed memory. See `do_shutdown()` in `src/simulate.c` for reference.

### CMake settings for unit test programs
- In `CMakeLists.txt`, link unit-test program to `GTest::gtest_main` and the specific libraries needed for the test (e.g., `async`, `socket`, `stem` for driver tests)
Test patterns:
  - Generic library tests (e.g., [test_logger](tests/test_logger/)) link only needed dependencies
  - Driver component tests link the `stem` object library (all driver code except main.c)
- For tests that require mudlib access, add a custom command to copy the testing mudlib in [examples/](examples/) to the build directory at POST_BUILD. Pattern:
  ```cmake
  add_custom_command(TARGET test_lpc_compiler POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/m3_mudlib ${CMAKE_CURRENT_BINARY_DIR}/m3_mudlib
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/examples/m3.conf ${CMAKE_CURRENT_BINARY_DIR}
  )
  ```
- Always set `DISCOVERY_TIMEOUT 20` to avoid cloud antivirus conflicts

### LPC Object Loading in Tests
Make sure `init_master()` is called in the fixture setup before loading LPC objects, as the master object is responsible for approving `load_object()` calls. Set `current_object` to `master_ob` when loading objects in tests to ensure the master applies to approve the load.

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

### Calling LPC functions from tests
1. Get the shared string for the function name using `findstring` (e.g., `findstring("create")`)
2. Call `find_function()` to get the function index from the program and the `program_t` pointer if the function is inherited from a parent program. Note that `find_function` requires pointer to a shared string returned from `findstring`. Pattern:
```cpp
// found_prog is the program where the function is defined.
// index is the COMPILER function index (position in found_prog's function_table).
// function index offset for inherited functions is returned via fio.
// variable index offset for inherited globals is returned via vio.
program_t* found_prog = find_function(prog, findstring("add"), &index, &fio, &vio);
ASSERT_NE(found_prog, nullptr);
```
3. If `found_prog` is different from `prog`, the function is inherited. The `index` can be used to obtain the `compiler_function_t` in `found_prog`'s function_table where function address and argument count are stored. **CRITICAL:** `index` is a compiler index, NOT a runtime index. Convert it using `found_prog->function_table[index].runtime_index` then add `fio` for inheritance offset: `(found_prog->function_table[index].runtime_index + fio)` gives the runtime dispatch index in `prog`.
4. Push arguments onto the stack using `push_number()`, `push_string()`, etc. Find available push functions in [src/stack.c](src/stack.c).
5. Call `call_function()` with the actual function runtime index and number of arguments already pushed onto the stack (`num_args`). Pattern:
```cpp
// Convert compiler index to runtime index, then add inheritance offset
int runtime_index = found_prog->function_table[index].runtime_index + fio;
// For void functions, ret can be a dummy svalue_t since it won't be used
svalue_t ret;
call_function (prog, runtime_index, num_args, &ret);
```

### Socket Testing Patterns

Tests using network sockets require special initialization handling:

**Header Includes**:
```cpp
#include <gtest/gtest.h>
extern "C" {
#include "async/async_runtime.h"
#include "port/socket_comm.h"  // Provides create_test_socket_pair()
}
```

**Windows Socket Initialization**:
- Make sure the fixture calls `WSAStartup()`/`WSACleanup()` if `WINSOCK` is defined. This can be done via a global test environment in GoogleTest. Pattern:
```cpp
#ifdef WINSOCK
class WinsockEnvironment : public ::testing::Environment {
    // SetUp() calls WSAStartup(), TearDown() calls WSACleanup()
};
static ::testing::Environment* const winsock_env =
    ::testing::AddGlobalTestEnvironment(new WinsockEnvironment);
#endif
```

## Unit Test File Organization

- The testing mudlib is copied to ${CMAKE_CURRENT_BINARY_DIR} so the unit-test programs of different configurations can be launched from this directory and locate the mudlib config file `m3.conf` from current working directory. Pattern:
```bash
cd out/build/linux/tests/test_lpc_compiler
./RelWithDebInfo/test_lpc_compiler
```
- Individual test files (e.g., `test_lpc_compiler.cpp`): Include common header, write tests,
- Common header `fixtures.hpp`: Define test fixtures for shared setup (e.g., `LPCCompilerTest` that initializes simulate subsystem). Use the `Test` suffix for fixture classes to follow GoogleTest conventions.
