Unit-Testing
=====
Neolith uses googletest (Google&trade; C++ Testing Framework) to do unit-testings.

Prerequistes:
~~~sh
sudo apt install libgtest-dev
~~~

If googletest is not found in the build environment, the unit-testing programs are skipped automatically.

> [!NOTE]
> MudOS and LPMud does not come with any unit-testing code.
> All the unit-testing code are added by the Neolith project and may not work with other forks of LPMud Driver.

## Running Tests
To run unit-tests, run the `ctest` command using preset:
~~~sh
ctest --preset ut-linux
~~~

## Adding Unit Tests
Unit-testing is an efficient way to automate tests and enhance software quality.
It also provides an source-controlled way to formalize required software behaviors with testing code.

To add unit-tests, create a subdirectory in `tests` and build an executable testing program for the testee package.

In the `CMakeLists.txt`, make sure these settings are added:
~~~cmake
target_link_libraries(test_package PRIVATE GTest::gtest_main)
gtest_discover_tests(test_package)
~~~

> [!TIP]
> DO NOT implement the `main()` function, it is provided by the `GTest::gtest_main` target and handles various command line options.

### What are the "units" ?
The original LPMud and MudOS code base was developed by dozens of different authors during a time span of decades.
Not every author has consistent view of "units" about the LPMud source code.
The inconsistency of design and coding style makes it difficulty to enhance or migrate the code to use modern C/C++ standard libraries, e.g. memory management, exception handling, portable filesystem access, ... etc.

In Neolith, the LPMud driver source code are re-organized into several static library units in the `/lib` directory and a collection of LPMud driver dependencies (CMake object library) in the `/src` directory.

- For **generic library units**, the unit-testing program only need to link necessary dependency targets.
  - See the [test_logger](/tests/test_logger/CMakeLists.txt) program for example.
- For **LPMud driver units**, the unit-testing program adds the `stem` link target to add the same list of object files for the `neolith` executable.
  - See the [test_lpc_compiler](/tests/test_lpc_compiler/CMakeLists.txt) program for example.

> [!TIP]
> Search the keyword `target_link_libraries` in the source tree to get a summary of **units** defined in the Neolith project.
>
> As we continue to clean up the legacy source code, it is possible that more units will be made available as generic library units and, replaced with modern implementations.

### Writing unit-testing code
For best utilization of googletest, you'll need to write the unit-testing code in **C++**.
Begin the `.cpp` file with some mandatory #include like below:
~~~cxx
#ifdef HAVE_CONFIG_H
#include <config.h> // configuration defines
#endif
extern "C" {
    #include "std.h" // standard includes
    // insert more testee unit's #include here
}
#include <gtest/gtest.h> // googletest

using namespace testing; // googletest
~~~

Note that the `extern "C"` block is necessary since the googletest code is in C++.
Implement your unit-testing code with `TEST()` or `TEST_F()` blocks. For example:
~~~cxx
TEST(TestSuite, testName) {
    // tests if test_func() returns 0
    EXPECT_EQ(test_func(), 0);
}
~~~

## Visual Studio Code Integration
The googletest framework works very well with the popular VS Code (using CMake Tools extension from Microsoft&trade;).
And of course, on Windows WSL.

This makes **Windows WSL + VS Code + CMake** a ideal cross-platform development environment for the Neolith LPMud Driver.

The unit-tests added this way can be found in the **Testing** panel on VS Code and provides an easy to use IDE for the Neolith project.

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

This will further enable the **Debug Test** icon to let you debug a failed unit-test.

> [!TIP]
> For using MSVC debugger in Windows, change `cppdbg` to `cppvsdbg`.
