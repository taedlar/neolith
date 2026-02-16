INSTALL
=======

# Setting Up Build Environment

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

# Building with CMake

Building Neolith with CMake is made simple via presets:
~~~sh
cmake --preset linux
cmake --build --preset ci-linux
~~~
The `neolith` executable can be found in `out/build/linux/src/RelWithDebInfo/`.

## Build Presets
The following build presets are defined for regular development tasks:

Preset|Configuration|Targets|Description
---|---|---|---
`dev-linux`|Debug|`all`|Performs typical `make`.<br/>Used for feature **development** and troubleshooting.
`pr-linux`|RelWithDebInfo|`all`|Performs typical `make`.<br/>Used for unit-testing on **pull-requests** before merge into the main trunk.
`ci-linux`|RelWithDebInfo|`all`<br/>Benchmarks|Performs `make clean` and then `make`.<br/>This is the **continous-integration** build that ensures clean re-build of all targets and dependencies.<br/>Usually used after a clean re-configuration to create so-called nightly build.

## Windows Platform
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
