Neolith LPMud Driver
====================
![C](https://img.shields.io/badge/C-%23ABCC.svg?style=plastic&logo=c&logoColor=white)
![C++](https://img.shields.io/badge/C++-%2300599C.svg?style=plastic&logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-%23008FBA.svg?style=plastic&logo=cmake&logoColor=white)

Neolith is an open-source project of [LPMud](https://en.wikipedia.org/wiki/LPMud) driver forked from the **MudOS v22pre5** code base.
Most of the efforts are to improve the code quality, code stytle consistency, portability, performance and other design issues.

The project goal is to provide mudlib developers and driver maintainers with a **minimalist code base** that keeps classic LPC workflows intact while making the driver easier to extend, port, and reason about.

## Features

- **Platforms**: Linux (primary), Windows (native MSVC and Clang-CL); Apple Clang planned.
- **Async Workers**: Blocking I/O (DNS, HTTP) runs on worker threads through a unified event loop — mudlib code stays single-threaded and the backend never freezes.
- **Console Mode**: Treats stdin/stdout as an interactive user; enables test automation, local debugging, and standalone [MUD applications](docs/manual/mud-application.md) without a telnet client.
- **UTF-8**: Strings are counted byte-spans. Wide literals, `explode(str, "")` by character, and wide-char `strsrch()` work correctly across all string operators.
- **JSON** (`PACKAGE_JSON`): `to_json()` / `from_json()` efuns with explicit UTF-8 and embedded-null handling; `from_json()` accepts `buffer` for large payloads.
- **CURL** (`PACKAGE_CURL`): Non-blocking HTTP requests (`perform_using()`, `perform_to()`, `in_perform()`) without blocking the backend.
- **Upgraded int / float / string**: `int` is 64-bit everywhere; `float` uses native `double` precision; `string` is a true counted byte-span preserving embedded nulls.
- **C99-Style Mixed Local Declarations** Local variables can be declared after statements inside any `{ ... }` block. See [LPC Guide](docs/manual/lpc.md#c99-style-local-declarations-neolith-extension) for details and current limits.
- **Driver Robustness**: LPC error handling migrated from `longjmp()` to C++ exceptions; heap allocation and string memory management hardened with RAII wrappers and const-correct APIs.

## How To Build

Neolith is always released as source code.
You need to build the Neolith executable from source code on your target platform.
See [INSTALL](docs/INSTALL.md) for detailed instructions on setting up build environment.

To utilize modern compiler toolchains and adding features from other packages, the build system of Neolith has been migrated to CMake.
Several CMake presets are added to enable WSL + VS Code development:
~~~sh
cmake --preset linux
cmake --build --preset ci-linux
~~~
If the build finishes successfully, the `neolith` executable can be found in `out/linux/src/RelWithDebInfo/`.

See [CMakePresets.json](CMakePresets.json) for details.

## Usage
To start a MUD using Neolith LPMud Driver, you need a Neolith configuration file.
Make a copy from [src/neolith.conf](src/neolith.conf) and customize the settings for your needs.
You can launch the MUD by the command:
~~~sh
neolith -f /path/to/neolith.conf
~~~

### Quick Start & Testing
Neolith includes a minimal mudlib testsuite in [examples/m3_mudlib/](examples/m3_mudlib/) and a pexpect-based testing framework in [examples/m3_testbots](examples/m3_testbots/).

~~~sh
# Start the M3 mudlib and login as console user
cd examples
/path/to/neolith -f m3.conf -c
~~~

For automated testing, first build Neolith with `pr-*` or `ci-*` preset, and run (requires [hatch](https://pypi.org/project/hatch/)):
~~~sh
# Run automated testbots against M3 mudlib using locally built LPMud driver
cd examples/m3_testbots
hatch run smoke_test
~~~

See [examples/README.md](examples/README.md) for comprehensive usage examples and troubleshooting.

If you are new to Neolith, the follow documentations are for you:
- [Neolith Administrator Guide](docs/manual/admin.md)
- [Neolith LPC Guide](docs/manual/lpc.md)

## Contributing
The decades-old C codes from MudOS/LPMud is quite messy and buggy in terms of modern C/C++ standards.
Neolith project intended to make a good minimalist LPMud code base where open source contributors can start their LPMud not only with mudlibs, but also extensions to the LPMud Driver.

Please take a look into the following documents before you jump in:
- [Neolith LPMud Driver Internals](docs/manual/internals.md)
- [Neolith Developor Reference](docs/manual/dev.md)

## License

Neolith is licensed under a **fair-code architecture** that balances permissive modification rights with non-commercial use requirements:

- **Permissive Modifications**: You may freely modify, refactor, and optimize the driver source code. There is no copyleft enforcement; your custom code changes remain entirely closed-source and proprietary if you choose.
- **Non-Commercial Use**: In accordance with the original copyright claims by Lars Pensjö, the driver may not be used for monetary gain or commercial purposes without explicit written permission from the original copyright holders.
- **Community Contribution**: While closed research forks and private deployments are fully permitted, contributors are encouraged to submit upstream pull requests to maintain core compatibility with the primary Neolith toolchain.

See [LICENSE](LICENSE) for the complete legal text.

## Credits

Neolith is a LPMud Driver that powered the [Eastern Stories 2 MUD](https://zh.wikipedia.org/wiki/%E6%9D%B1%E6%96%B9%E6%95%85%E4%BA%8B2_%E5%A4%A9%E6%9C%9D%E5%B8%9D%E5%9C%8B) (running since 1995).
The code was modified from MudOS v22pre5, which is derived from the original LPMud by Lars Pensjö.
Credits to original authors can be found in [Credits.LPmud](docs/Credits.LPmud) and [Credits.MudOS](docs/Credits.MudOS).

