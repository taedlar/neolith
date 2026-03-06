INSTALL
=======

## Setting Up Build Environment

The recommended development environment is Ubuntu 20.04 LTS (including [WSL](https://learn.microsoft.com/en-us/windows/wsl/install)).

You also need the following packages:
~~~sh
sudo apt install build-essential
sudo apt install ninja-build
sudo apt install bison
~~~

For Microsoft Visual Studio, the BISON tool can be installed via `winget`:
~~~cmd
winget install gnuwin32.bison
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

### Dependencies for Additional Features
The CMake build scripts detects availability of packages and enable additional features:

### `openssl`
The popular OpenSSL library provides modern cryptography for network communications as well as HTTPS.
You'll need both `openssl` and `libssl-dev` to enable those features requires OpenSSL:
~~~bash
sudo apt-get install openssl libssl-dev
~~~

For Windows, you need to specify the `OPENSSL_ROOT_DIR` in the `cacheVariables` of `windows-default` preset.

### `curl`
CURL is the most popular tool and library to connect an application and various **cloud** infrastructure with REST APIs.
CURL also requires OpenSSL to deal with HTTPS stuff and protect the data transmitted:
~~~bash
sudo apt-get install curl libcurl4-openssl-dev
~~~

### `boost`
Boost is a powerful **C++** library that provides open source, peer-reviewed, and portable code tend to be de facto C++ standards.
While LPMud was first developed with C language, migrating to portable C++ gradually align with our goal to modernize the codebase in minimalist way.

To keep a small footprint, we'll start from the core Boost libraries (avoid rarely used Boost libraries in `libboost-all-dev`):
~~~bash
sudo apt-get install libboost-dev
~~~

For Windows, you need to specify the `BOOST_ROOT` in the `cacheVariables` of `windows-default` preset.
