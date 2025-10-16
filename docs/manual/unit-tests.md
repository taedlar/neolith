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
Unit-tests is an efficient way to automate testings and enhance software quality.
It also provides an efficient way to define required software behaviors with code.

To add unit-tests, create a subdirectory in `tests` and build an executable testing program for the testee package.

In the `CMakeLists.txt`, make sure these settings are added:
~~~cmake
target_link_libraries(test_package PRIVATE GTest::gtest_main)
gtest_discover_tests(test_package)
~~~

> [!TIP]
> DO NOT implement the `main()` function, it is provided by the `GTest::gtest_main` target and handles various command line options.

Implement your unit-testing code with `TEST()` or `TEST_F()` blocks. For example:
~~~cxx
TEST(package, test_name)
{
    // tests if test_func() returns 0
    EXPECT_EQ(test_func(), 0);
}
~~~

## Visual Studio Code Integration
The googletest framework works very well with the popular VS Code (using CMake Tools extension from Microsoft&trade;).
And of course, on Windows WSL.

This makes **Windows WSL + VS Code + CMake** a ideal cross-platform development environment for the Neolith LPMud Driver.

The unit-tests added this way can be found in the **Testing** panel on VS Code and provides an easy to use IDE for the Neolith project.
