INSTALL
=======

## Setting Up Build Environment

The recommended development environment is Ubuntu 20.04 LTS or later.

> [!TIP]
> For Windows users, the build is also tested with WSL (Windows Subsystem for Linux) on Windows 10.  
> See [Install Linux on Windows with WSL](https://learn.microsoft.com/en-us/windows/wsl/install) for instructions to install Ubuntu on Windows.

You also need the following packages:
~~~sh
sudo apt install build-essential
sudo apt install ninja-build
sudo apt install bison
~~~

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

Neolith can be successfully build with **Visual Studio 2019** :tada: and **Clang/LLVM** :tada: :
~~~sh
# configure build with Visual Studio 2019 (vc++ 16)
cmake --preset vs16-x64

# configure build with Clang/LLVM, available with Visual Studio 2019 v16.2 and later
cmake --preset clang-x64
~~~

The build presets follow the same naming as in Linux build:
- `dev-` for development build
- `pr-` for pull-request validation build
- `ci-` for continous-integration (nightly build)

> [!Note]
> There are still some minor portability issues to be fix on MSVC and Clang.

## Dependencies for Optional Features
The CMake build scripts detects availability of packages and enable optional features:

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

## Using CMake dependency provider to satisfy requirements for optional features
On Windows, there is no standard package manager to install dependency libraries in conventional locations such as `/usr/lib` as on Linux.
CMake offers **dependency provider** since v3.24 to allow the `find_package()` requests to be intercepted and handled to satisfy dependencies before returning "not found".

If the following CMake variables are defined, they enable the configure step to attempt fetching source code and build the dependencies locally:

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
