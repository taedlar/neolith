INSTALL
=======

## Setting Up Build Environment

### Prerequisites
For UNIX-like OS, you need the following packages 
~~~sh
# Using Ubuntu for example:
sudo apt install build-essential
sudo apt install ninja-build
sudo apt install bison
~~~

For Windows platform:
- **Option 1**: Native Win32 (x64) build using Microsoft Visual Studio 2019:
  - Install [Bison for Windows](https://gnuwin32.sourceforge.net/packages/bison.htm) and provide the install location via `BISON_ROOT` variable in `CMakePresets.json`.
  - (Optional) Fetch **GoogleTest** using [FETCH_GOOGLETEST_FROM_SOURCE](#fetch_googletest_from_source) when configuring the build.
  - (Optional) If **OpenSSL** and related features are desired, install [Strawberry Perl](https://strawberryperl.com/) for required toolchains to build OpenSSL on Windows.
- **Option 2**: Using WSL:
See [Install Linux on Windows with WSL](https://learn.microsoft.com/en-us/windows/wsl/install) for instructions to install Ubuntu on Windows.

### CMake
Neolith uses **CMake** to organize the build and manage dependencies. You need cmake **v3.28** or later to build the project:
~~~bash
sudo apt install cmake
~~~

If your Linux distribution's cmake version is too old, visit [Download CMake](https://cmake.org/download/) to download a newer version.

## Dependencies for Optional Features
The CMake build scripts detects availability of packages and enable optional features:

### `GoogleTest`
Neolith uses GoogleTest for unit-testing.
If you don't plan to modify the source code, this is optional.
~~~bash
# For Linux
sudo apt install libgtest-dev
~~~

For using fetched GoogleTest source code of specific version, see [FETCH_GOOGLETEST_FROM_SOURCE]()

### `OpenSSL`
The popular OpenSSL library provides modern cryptography for network communications as well as HTTPS.
You'll need both `openssl` and `libssl-dev` to enable those features requires OpenSSL:
~~~bash
# For Linux
sudo apt-get install openssl libssl-dev
~~~

For using local built OpenSSL binaries of specific version, see [FETCH_OPENSSL_FROM_SOURCE](#fetch_openssl_from_source) section below.

### `cURL`
CURL is the most popular tool and library to connect an application and various cloud infrastructure with **REST APIs**.
CURL also requires OpenSSL to deal with HTTPS protocol and protect the data transmitted:
~~~bash
# For Linux
sudo apt-get install curl libcurl4-openssl-dev
~~~

For using local built CURL binaries of specific version, see [FETCH_CURL_FROM_SOURCE](#fetch_curl_from_source) section below.

### `Boost`
Boost is a powerful **C++** library that provides open source, peer-reviewed, and portable code tend to be de facto C++ standards.
While LPMud was first developed with C language, migrating to portable C++ gradually align with our goal to modernize the codebase in minimalist way.

To keep a small footprint, we'll start from the core Boost libraries (avoid rarely used Boost libraries in `libboost-all-dev`):
~~~bash
# For Linux
sudo apt-get install libboost-dev
~~~

For Windows, Boost is available for download as [source code](https://sourceforge.net/projects/boost/) or [binaries](https://sourceforge.net/projects/boost/files/boost-binaries/). Supply the install location (e.g. `D:\boost_1_90_0`)with `BOOST_ROOT` variable in `CMakePresets.json` and allow the configure step to import Boost headers and binaries.

## Using Dependency Provider
CMake offers **dependency provider** since v3.24 to allow the `find_package()` requests to be intercepted and handled to satisfy dependencies before returning "not found".

If the following CMake variables are defined, they enable the configure step to attempt fetching source code and build the dependencies locally.
(The order of finding system library and using fetched source code is determined by the setting of [`FETCHCONTENT_TRY_FIND_PACKAGE_MODE`](https://cmake.org/cmake/help/v3.29/module/FetchContent.html#variable:FETCHCONTENT_TRY_FIND_PACKAGE_MODE) variable)

### `FETCH_GOOGLETEST_FROM_SOURCE`
If `FETCH_GOOGLE_TEST_FROM_SOURCE` is defined, it is used as a git tag to fetch GoogleTest source code from the official repository.
Example:
~~~bash
# Configure with fetched GoogleTest on Windows
cmake --preset vs16-x64 -DFETCH_GOOGLETEST_FROM_SOURCE=v1.17.0
# Configure with fetched GoogleTest on Linux
cmake --preset linux -DFETCH_GOOGLETEST_FROM_SOURCE=v1.17.0
~~~

### `FETCH_OPENSSL_FROM_SOURCE`
When `FETCH_OPENSSL_FROM_SOURCE` is defined, it is used as a git tag to fetch OpenSSL source code from the official repository (see [prebuild-openssl.cmake](../cmake/prebuild-openssl.cmake)) for satisfying the `OpenSSL`. Example:
~~~bash
# Configure and prebuild OpenSSL from source code using MSVC
cmake --preset vs16-x64 -DFETCH_OPENSSL_FROM_SOURCE=openssl-3.6.1
# Configure and prebuild OpenSSL from source code on Linux
cmake --preset linux -DFETCH_OPENSSL_FROM_SOURCE=openssl-3.6.1
~~~

> [!IMPORTANT]
> **Agent Instructions — OpenSSL Prebuild**
>
> When assisting with a build that uses `FETCH_OPENSSL_FROM_SOURCE`, follow these steps before taking any action that would trigger the OpenSSL prebuild:
>
> 1. **Check `OPENSSL_ROOT_DIR`**: `prebuild-openssl.cmake` sets `OPENSSL_ROOT_DIR` to
>    `${CMAKE_BINARY_DIR}/openssl` (e.g. `out/build/<preset>/openssl`).
>    If that directory already exists, OpenSSL is fully prebuilt and installed — no build action
>    is needed. Skip to using `find_package(OpenSSL)` directly.
>
> 2. **Check `configdata.pm`**: If `OPENSSL_ROOT_DIR` does not exist, check whether
>    `${CMAKE_BINARY_DIR}/_deps/openssl-build/configdata.pm` exists.
>    - If present, OpenSSL has already been configured (but not installed). The build can
>      resume from the compile step without re-running the `Configure` script.
>    - If absent, a full configure + compile + install will be required.
>
> 3. **Ask for user consent before building**: Compiling OpenSSL from source is a long-running
>    operation (potentially 10–30 minutes depending on the host). **Always ask the user for explicit
>    confirmation** before running any CMake configure or build command that would trigger the
>    OpenSSL prebuild. Describe which step will run (configure, compile, or install) and warn
>    that it will take significant time.
>
> 4. **MSVC x64 build environment**: On Windows, OpenSSL contains assembly code that requires x64 MSVC toolchains
>    to produce correct object files for linking. If link error occurs, it is possible
>    the x64 build environment is not setup correctly.
>    Try starting a new shell and run `"%vcinstalldir%\auxiliary\build\vcvars64.bat"`
>    before doing a clean re-build. 

### `FETCH_CURL_FROM_SOURCE`
When `FETCH_CURL_FROM_SOURCE` is defined, it is used as a git tag to fetch cURL source code from the official repository and build it as part of the main CMake build.

In this project, fetched cURL is integrated into the normal configure/build flow (via dependency provider + `FetchContent`). There is no separate manual prebuild step.

Example:
~~~bash
# Configure with fetched cURL source code using MSVC
cmake --preset vs16-x64 -DFETCH_CURL_FROM_SOURCE=curl-8_19_0
# Configure with fetched cURL source code on Linux
cmake --preset linux -DFETCH_CURL_FROM_SOURCE=curl-8_19_0
~~~

> [!IMPORTANT]
> **Agent Instructions — cURL Fetch/Build**
>
> When assisting with a build that uses `FETCH_CURL_FROM_SOURCE`, follow these guidelines:
>
> 1. **No prebuild consent gate needed**: Unlike OpenSSL prebuild, fetched cURL is built in-tree with the main project and does not require a separate long-running prebuild confirmation step.
>
> 2. **Use normal project build flow**: Configure and build using project presets/targets. Do not add custom standalone cURL build commands unless explicitly requested.
>
> 3. **Avoid DLL side effects on tests**: `BUILD_SHARED_LIBS` is a global cache variable shared across dependencies. Keep it explicitly controlled to avoid unintentionally switching GoogleTest to DLL builds (`gtest.dll` / `gtest_main.dll`).

## Building with CMake

Building Neolith with CMake is made simple via presets:
~~~sh
cmake --preset linux
cmake --build --preset ci-linux
~~~
The `neolith` executable can be found in `out/build/linux/src/RelWithDebInfo/`.

### Build Presets
The following build presets are defined for regular development tasks:

Preset|Configuration|Targets|Description
---|---|---|---
`dev-linux`|Debug|`all`|Performs typical `make`.<br/>Used for feature **development** and troubleshooting.
`pr-linux`|RelWithDebInfo|`all`|Performs typical `make`.<br/>Used for unit-testing on **pull-requests** before merge into the main trunk.
`ci-linux`|RelWithDebInfo|`all`<br/>Benchmarks|Performs `make clean` and then `make`.<br/>This is the **continous-integration** build that ensures clean re-build of all targets and dependencies.<br/>Usually used after a clean re-configuration to create so-called nightly build.

### Windows Platform
Original MudOS and LPMud source code "probably" can build with mingw or Cygwin.

Neolith can build with **Visual Studio 2019** or **ClangCL/LLVM**:
~~~powershell
# configure build with Visual Studio 2019 (version 16.x)
cmake --preset vs16-x64
# configure build with Clang/LLVM, available with Visual Studio 2019 (version 16.2 and later)
cmake --preset clang-x64
~~~

On Windows, there is no standard package manager to install dependency libraries in conventional locations such as `/usr/lib` on Linux.
You may neeed to add appropriate `FETCH_*_FROM_SOURCE` settings to download and build dependencies.

### Reference Configuration
For Linux:
~~~powershell
cmake "-DFETCH_GOOGLE_FROM_SOURCE=v1.17.0" "-DFETCH_OPENSSL_FROM_SOURCE=openssl-3.6.1" "-DFETCH_CURL_FROM_SOURCE=curl-8_19_0" --preset linux
~~~

For Windows:
~~~powershell
cmake "-DFETCH_GOOGLE_FROM_SOURCE=v1.17.0" "-DFETCH_OPENSSL_FROM_SOURCE=openssl-3.6.1" "-DFETCH_CURL_FROM_SOURCE=curl-8_19_0" --preset vs16-x64
~~~
