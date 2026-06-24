Unit-Testing
=====
Neolith uses **GoogleTest** as unit-testing framework.

If GoogleTest is not found in the build environment, the configure step downloads GoogleTest from the GitHub.

## Prerequisites

Unit-Testing is enabled by default. To skip building unit-tests entirely:
~~~bash
cmake --preset <configure-preset> -DBUILD_TESTING=OFF
~~~

## Running Tests
To run unit-tests, run the `ctest` command using preset:
~~~sh
# Show test presets
ctest --list-presets

# Run unit-tests. For example:
ctest --preset ut-vs16-x64
~~~

## Adding Unit Tests
Unit-testing is an efficient way to automate tests and enhance software quality.
It also provides a source-controlled way to formalize required software behaviors with testing code.

> [!IMPORTANT]
> MudOS and LPMud does not come with any unit-testing code.
> All the unit-testing code are specific to Neolith.
> For Efun behaviors, the testing code are mostly derived from Efun docs to keep compatibility with MudOS.
> For internal functions and driver architecture, the testing code are for locking down function contracts.

To add unit-tests, create a subdirectory in `tests` and build an executable testing program for the testee package.

In the `CMakeLists.txt`, make sure these settings are added:
~~~cmake
target_link_libraries(test_package PRIVATE GTest::gtest_main)
# default discovery timeout was 5 seconds, may clash with some cloud-based antivirus
gtest_discover_tests(test_package DISCOVERY_TIMEOUT 20)
~~~

> [!TIP]
> DO NOT implement the `main()` function, use the `GTest::gtest_main` target which handles command line options and integrates with IDE.

### What are the "units" ?
The original LPMud and MudOS code base was developed by dozens of different authors during a time span of decades.
Not every author has consistent view of "units" about the LPMud source code.
The inconsistency of design and coding style makes it difficult to enhance or migrate the code to use modern C/C++ standard libraries, e.g. memory management, exception handling, portable filesystem access, ... etc.

In Neolith, the LPMud driver source code are re-organized into several static library units in the `/lib` directory and a collection of LPMud driver dependencies (CMake object library) in the `/src` directory.

- For **generic libraries**, the unit-testing program only need to link necessary dependency targets.
  - See the [test_logger](/tests/test_logger/CMakeLists.txt) program for example.
- For **LPMud driver components**, the unit-testing program links with the `stem` CMake target (the object files that form the `neolith` executable).
  - See the [test_lpc_compiler](/tests/test_lpc_compiler/CMakeLists.txt) program for example.

### Writing unit-testing code
Googletest requires unit-testing code written in **C++**.
Neolith has inserted necessary `extern "C"` guards in all headers to enable unit-testing.

Begin a unit testing implementation file with mandatory headers like below:
~~~cxx
#ifdef HAVE_CONFIG_H
#include <config.h> // project configurations, must come first
#endif

#include "src/std.h" // for cross-platform stuff
// <--- insert more testee headers here

#include <gtest/gtest.h> // gtest main header

using namespace testing; // to access gtest helpers
~~~

Implement testing code with `TEST()` or `TEST_F()` blocks and write assertions. For example:
~~~cxx
TEST(TestSuite, testName) {
    EXPECT_EQ(test_func(), 0) // assertion
        << "test_func() returns non-zero"; // if failed, print this
}
~~~

## Visual Studio Code Integration
The googletest framework is well supported by the popular VS Code (using CMake Tools extension from Microsoft). The **Windows WSL + VS Code + CMake** combination provides the best build speed and testing environment for Neolith.

Test cases added via `TEST()` and `TEST_F()` can be discovered automatically and shown in the **Testing** panel of VS Code for easy access.
This can save a lot of typing when running particular test.

### Debugging with CTest and VSCode
You can run individual unit-test by clicking the **Run Test** icon from the "Testing" tab of VSCode.

When an unit-test fails, it is desirable to run the unit-test in debugger to find out what goes wrong.

Add a configuration like below in your `.vscode/launch.json`:
~~~json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "CTest Debug Test",
            "type": "cppdbg",
            "request": "launch",
            "program": "${cmake.testProgram}",
            "args": ["${cmake.testArgs}"],
            "cwd": "${cmake.testWorkingDirectory}"
        }
    ]
}
~~~

> [!NOTE]
> For using MSVC debugger in Windows, change `cppdbg` to `cppvsdbg`.

This will further enable the **Debug Test** icon to let you debug test failure.

