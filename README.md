Neolith LPMud Driver
====================
![C](https://img.shields.io/badge/C-%23ABCC.svg?style=plastic&logo=c&logoColor=white)
![C++](https://img.shields.io/badge/C++-%2300599C.svg?style=plastic&logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-%23008FBA.svg?style=plastic&logo=cmake&logoColor=white)

Neolith is an open-source project of [LPMud](https://en.wikipedia.org/wiki/LPMud) driver based on the **MudOS v22pre5** code base.
Most of the efforts are to improve the code quality, portability, performance and other design issues.
The goal is to provide LPMud builders a code base with concise design and easy-to-read source code while also easy to maintain and troubleshoot.

## Supported Platforms
Platform | autotools build | CMake build
--- | --- | ---
Linux | Yes | Yes (gcc)
Windows | No | WIP (MSVC)

## Credits & License
Neolith is a LPMud Driver that run the [Eastern Stories 2 MUD](https://zh.wikipedia.org/wiki/%E6%9D%B1%E6%96%B9%E6%95%85%E4%BA%8B2_%E5%A4%A9%E6%9C%9D%E5%B8%9D%E5%9C%8B) (up since 1995).
The code was modified from MudOS v22pre5 which is derived from the original LPMud by Lars Pensjö.
Credits to original authors can be found in [Credits.LPmud](docs/Credits.LPmud) and [Credits.MudOS](docs/Credits.MudOS).

The Neolith project is intended to be distributed under [GPLv2](docs/GPLv2_LICENSE), with the copyright notices from original authors of LPMud and MudOS still applies.

> [!IMPORTANT]
> Although GPLv2 allows commercial use, this project contains additional restrictions from original authors.
> - "May not be used in any way whatsoever for monetary gain" (restriction by Lars Pensjö, origin of LPMud)
> - GPLv2 (open source required, must comply all restrictions from all authors)
>
> With all these terms combined, **Commercial Use is NOT ALLOWED**.

See [Copyright](Copyright) for details.

## How To Build

Neolith is always released as source code.
You need to build the Neolith executable from source code on your target platform.
See [INSTALL](INSTALL.md) for detailed instructions on setting up build environment.

### Out-of-tree build using autotools
After checkout the source code, do `bootstrap` to get autotools ready.

Create a `build` subdirectory, and run `configure` from the build directory:
~~~sh
mkdir build
cd build && ../configure
make
~~~
If the build finishes successfully, the `neolith` executable can be found in `build/src`.

### CMake build (WIP)
To utilize modern compiler toolchains and adding features from other packages, the build system will be migrated to CMake.
Several CMake presets are added to enable WSL + VS Code development:
~~~sh
cmake --preset linux
cmake --build --preset ci-linux
~~~
If the build finishes successfully, the `neolith` executable can be found in `out/linux/src/RelWithDebInfo`.

See [CMakePresets.json](CMakePresets.json) for details.

## Usage
To start a MUD using Neolith LPMud Driver, you need a Neolith configuration file.
Make a copy from [src/neolith.conf](src/neolith.conf) and customize the settings for your needs.
You can launch the MUD by the command:
```sh
neolith -f /path/to/neolith.conf &
```
If you are new to Neolith, the follow documentations are for you:
- [Neolith Administrator Guide](docs/manual/admin.md)
- [Neolith LPC Guide](docs/manual/lpc.md)
- [Neolith World Creation Guide](docs/manual/world.md)

## Contributing
An open source project relies on collaboration of contributors to fix bugs and improve the code quality.
The original code base from MudOS is quite messy and buggy in terms of nowadays standards of open source community.
We hope the Neolith project to be a good place where open source contributors are comfortable to read the code and/or contribute new enhancements.
Therefore developer-oriented documentations shall be kept up-to-date as best as possible.

Please take a look into the following documents before you jumping-in:
- [Neolith LPMud Driver Internals](docs/manual/internals.md)
- [Neolith Developor Reference](docs/manual/dev.md)
