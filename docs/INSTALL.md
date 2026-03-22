INSTALL
=======

## Setting Up the Build Environment

### Prerequisites

For Linux (Ubuntu example):

~~~sh
sudo apt install build-essential ninja-build bison cmake
~~~

Neolith requires CMake 3.28 or later.
If your distribution provides an older version, install a newer one from [cmake.org](https://cmake.org/download/).

For Windows:

- Option 1: Native build with Visual Studio 2019 (recommended)
  - Install [Bison for Windows](https://gnuwin32.sourceforge.net/packages/bison.htm).
  - Ensure `BISON_ROOT` points to the Bison install root (for example `C:/GnuWin32`). The default in [CMakePresets.json](../CMakePresets.json) already uses this path.
  - Optional: install [Strawberry Perl](https://strawberryperl.com/) if you plan to use `FETCH_OPENSSL_FROM_SOURCE`.
- Option 2: WSL
  - Follow [Install Linux on Windows with WSL](https://learn.microsoft.com/en-us/windows/wsl/install), then use the Linux instructions.

## Optional Dependencies

The build system enables optional features when dependencies are found.

### GoogleTest

GoogleTest is only required for unit tests. To skip test targets entirely, configure with:

~~~sh
cmake --preset <configure-preset> -DBUILD_TESTING=OFF
~~~

Linux package:

~~~sh
sudo apt install libgtest-dev
~~~

You can also fetch GoogleTest from source with [FETCH_GOOGLETEST_FROM_SOURCE](#fetch_googletest_from_source).

### OpenSSL

OpenSSL enables modern cryptography and HTTPS support.

Linux packages:

~~~sh
sudo apt install openssl libssl-dev
~~~

You can also fetch OpenSSL from source with [FETCH_OPENSSL_FROM_SOURCE](#fetch_openssl_from_source).

### cURL

cURL enables outbound HTTP(S) and REST API integration.

Linux packages:

~~~sh
sudo apt install curl libcurl4-openssl-dev
~~~

You can also fetch cURL from source with [FETCH_CURL_FROM_SOURCE](#fetch_curl_from_source).

### Boost

Boost is optional and used when available.

Linux package:

~~~sh
sudo apt install libboost-dev
~~~

On Windows, install Boost manually and set `BOOST_ROOT` (for example `D:\boost_1_90_0`) in your preset or configure command.

### c-ares

c-ares enables async DNS resolution for built-in socket-connect hostname support. When enabled via `PACKAGE_PEER_REVERSE_DNS`, c-ares provides a bounded DNS worker pool with admission control and flood protection.

Linux package:

~~~sh
sudo apt install libc-ares-dev
~~~

You can also fetch c-ares from source with [FETCH_CARES_FROM_SOURCE](#fetch_cares_from_source).

## Dependency Provider Settings

This project uses CMake dependency provider hooks (via `cmake/setup.cmake`) so `find_package()` can be satisfied by FetchContent-based sources when requested.

If any of the following cache variables are set, configure will fetch that dependency source at the specified tag and make it available to the build.
Resolution order still depends on `FETCHCONTENT_TRY_FIND_PACKAGE_MODE`.

### `FETCH_GOOGLETEST_FROM_SOURCE`

If defined, this value is used as the Git tag for GoogleTest.

~~~sh
# Windows
cmake --preset vs16-x64 -DFETCH_GOOGLETEST_FROM_SOURCE=v1.17.0

# Linux
cmake --preset linux -DFETCH_GOOGLETEST_FROM_SOURCE=v1.17.0
~~~

### `FETCH_OPENSSL_FROM_SOURCE`

If defined, this value is used as the Git tag for OpenSSL. OpenSSL is prebuilt and installed under `${CMAKE_BINARY_DIR}/openssl` by [prebuild-openssl.cmake](../cmake/prebuild-openssl.cmake), then resolved by `find_package(OpenSSL)`.

~~~sh
# Windows
cmake --preset vs16-x64 -DFETCH_OPENSSL_FROM_SOURCE=openssl-3.6.1

# Linux
cmake --preset linux -DFETCH_OPENSSL_FROM_SOURCE=openssl-3.6.1
~~~

> [!IMPORTANT]
> **Operational Notes for `FETCH_OPENSSL_FROM_SOURCE`**
>
> 1. If `${CMAKE_BINARY_DIR}/openssl` already exists, OpenSSL is already prebuilt and installed for that build tree.
> 2. If `${CMAKE_BINARY_DIR}/_deps/openssl-build/configdata.pm` exists, OpenSSL is already configured and can resume from compile/install.
> 3. OpenSSL source builds can take 10-30 minutes depending on host performance.
> 4. On Windows, ensure an x64 MSVC build environment when building x64 targets (for example via `vcvars64.bat`) to avoid architecture/link mismatches.

### `FETCH_CURL_FROM_SOURCE`

If defined, this value is used as the Git tag for cURL. In this project, cURL is fetched and built in-tree as part of the normal configure/build flow.

~~~sh
# Windows
cmake --preset vs16-x64 -DFETCH_CURL_FROM_SOURCE=curl-8_19_0

# Linux
cmake --preset linux -DFETCH_CURL_FROM_SOURCE=curl-8_19_0
~~~

> [!IMPORTANT]
> **Operational Notes for `FETCH_CURL_FROM_SOURCE`**
>
> 1. There is no separate manual prebuild phase for cURL.
> 2. Use normal Neolith presets and targets.
> 3. Keep `BUILD_SHARED_LIBS` controlled; it is global and can affect downstream dependencies such as GoogleTest.

### `FETCH_CARES_FROM_SOURCE`

If defined, this value is used as the Git tag for c-ares. c-ares is fetched and built in-tree as part of the normal configure/build flow.

~~~sh
# Windows
cmake --preset vs16-x64 -DFETCH_CARES_FROM_SOURCE=v1.34.6

# Linux
cmake --preset linux -DFETCH_CARES_FROM_SOURCE=v1.34.6
~~~

> [!IMPORTANT]
> **Operational Notes for `FETCH_CARES_FROM_SOURCE`**
>
> 1. There is no separate manual prebuild phase for c-ares.
> 2. Use normal Neolith presets and targets.
> 3. c-ares is only used when `PACKAGE_PEER_REVERSE_DNS` is enabled at build time (see [config.h.in](../config.h.in)).
> 4. When c-ares is available, async DNS resolution in `socket_connect()` is automatically enabled for hostname support.

## Building with CMake Presets

From repository root:

~~~sh
cmake --preset <configure-preset>
cmake --build --preset <build-preset>
~~~

Example (Linux CI-style clean rebuild):

~~~sh
cmake --preset linux
cmake --build --preset ci-linux
~~~

For Linux `RelWithDebInfo`, the executable is typically at `out/build/linux/src/RelWithDebInfo/neolith`.

### Build Presets Overview

Preset|Configuration|Targets|Description
---|---|---|---
`dev-linux`|Debug|`all`|Incremental development build.
`pr-linux`|RelWithDebInfo|`all`|Incremental validation build.
`ci-linux`|RelWithDebInfo|`all`|Clean rebuild (`cleanFirst=true`) for CI-style checks.

Windows and ClangCL equivalents:

- `dev-vs16-x64`, `pr-vs16-x64`, `ci-vs16-x64`
- `dev-clang-x64`, `pr-clang-x64`, `ci-clang-x64`
- `dev-vs16-win32`, `pr-vs16-win32`, `ci-vs16-win32`

### Running Unit Tests

Use CTest presets after building:

~~~sh
ctest --preset ut-linux
ctest --preset ut-vs16-x64
ctest --preset ut-clang-x64
~~~

## Reference Configure Commands

Linux:

~~~sh
cmake --preset linux \
  -DFETCH_GOOGLETEST_FROM_SOURCE=v1.17.0 \
  -DFETCH_OPENSSL_FROM_SOURCE=openssl-3.6.1 \
  -DFETCH_CURL_FROM_SOURCE=curl-8_19_0 \
  -DFETCH_CARES_FROM_SOURCE=v1.34.6
~~~

Windows (PowerShell):

~~~powershell
cmake --preset vs16-x64 `
  -DFETCH_GOOGLETEST_FROM_SOURCE=v1.17.0 `
  -DFETCH_OPENSSL_FROM_SOURCE=openssl-3.6.1 `
  -DFETCH_CURL_FROM_SOURCE=curl-8_19_0 `
  -DFETCH_CARES_FROM_SOURCE=v1.34.6
~~~
