Neolith LPMud Driver
====================
![C](https://img.shields.io/badge/C-%23ABCC.svg?style=plastic&logo=c&logoColor=white)
![C++](https://img.shields.io/badge/C++-%2300599C.svg?style=plastic&logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-%23008FBA.svg?style=plastic&logo=cmake&logoColor=white)

Neolith is an open-source project of [LPMud](https://en.wikipedia.org/wiki/LPMud) driver forked from the **MudOS v22pre5** code base.
Most of the efforts are to improve the code quality, code stytle consistency, portability, performance and other design issues.

The project goal is to provide mudlib developers and driver maintainers with a **minimalist code base** that keeps classic LPC workflows intact while making the driver easier to extend, port, and reason about.

## Features
### Supported Platforms
- Conventionally, Linux is the primary development platform for LPMud.
- Neolith has done a heavy clean up effort on MudOS codebase to enable native MSVC build (not Mingw or Cygwin).
- Clang-CL build is also supported now.
- Apple Clang is on the plan, but not yet started.

### Asynchronous Workers

Neolith's event-driven architecture offloads blocking I/O such as DNS resolution and HTTP work to worker threads through a **unified event loop**, so mudlib code can keep using the normal single-threaded LPC model without freezing the backend. Key differentiators:

- **Unified Event Loop**: Single `async_runtime_wait()` demultiplexes both I/O and worker completions
- **Main Thread Single Blocking Site**: Non-blocking queue operations and timeouts guarantee responsiveness on LPMud backend (commands, heart beats, reset ... etc.)
- **Zero Interpreter Coupling**: Workers never touch LPC state; results are self-contained
- **Platform Portable**: Seamless IOCP (Winsock) / epoll (Linux sockets) / poll (fallback) backends

**Current Use Cases**: DNS resolution (no driver freeze), console input with testbot automation, CURL efuns, foundation for future async features (REST APIs, GUI clients).

### Console Mode
Console mode lets Neolith treat standard input and output as a connected interactive user, so mudlib code can run without a telnet client or socket frontend. That makes it useful not only for deterministic test automation and local debugging, but also for instrumented mudlibs and VM-like CLI applications that use the LPC object model, command loop, and input APIs as a standalone application platform.

### UTF-8 Support
Neolith stores LPC strings as counted multi-byte strings and is designed for UTF-8 locales. For mudlib code, that means Unicode text can be handled without dropping back to raw C-string rules: wide string literals are validated at compile time, `explode(str, "")` can split UTF-8 text into characters, and `strsrch()` accepts wide-character search input while still returning byte offsets that match LPC range operators.

### JSON Support
When built with `PACKAGE_JSON`, mudlib code gets `to_json()` and `from_json()` efuns for moving LPC arrays, mappings, strings, ints, floats, and `undefined` values across JSON boundaries. `from_json()` also accepts `buffer` input for large payloads, and the JSON boundary is explicit about UTF-8 validation, Unicode escape handling, and embedded `\0` behavior.

### CURL Support
When built with `PACKAGE_CURL`, mudlib objects can configure and launch non-blocking HTTP requests with `perform_using()`, `perform_to()`, and `in_perform()`. Request state is stored per object, transfers run without blocking the backend, and the driver draws a clear line between text options and binary request bodies so outbound integrations stay predictable.

### Upgraded LPC string, int, float
Neolith upgrades the LPC runtime data model in ways that matter directly to mudlib code. LPC `int` is consistently 64-bits on every platform instead of depending on the host `long` size, LPC `float` now uses native `double` precision, taking advantage of 64-bits platform without increasing the storage cost of each LPC value because the payload already lives in a pointer-sized union. LPC `string` is a true counted byte-span value (similar to `std::string_view`) rather than implicit C strings, and string operators preserve that model instead of silently truncating values at the first embedded null byte.

### Driver Robustness Enhancement
- Migrated LPC error handling from `longjmp()` to C++ exceptions.
- Harden heap allocation with C++ RAII wrappers and integrate with C++ stack unwinding.
- Harden string memory management with semantic-explicit wrappers and const correctness API contract.

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
Neolith includes a minimal test mudlib in [examples/m3_mudlib/](examples/m3_mudlib/) and automated testing tools:
~~~sh
# Interactive testing
cd examples
/path/to/neolith -f m3.conf

# Automated testing
cd examples
python testbot.py
~~~

See [examples/README.md](examples/README.md) for comprehensive usage examples and troubleshooting.

If you are new to Neolith, the follow documentations are for you:
- [Neolith Administrator Guide](docs/manual/admin.md)
- [Neolith LPC Guide](docs/manual/lpc.md)
- [Neolith World Creation Guide](docs/manual/world.md)

## Contributing
The decades-old C codes from MudOS/LPMud is quite messy and buggy in terms of modern C/C++ standards.
Neolith project intended to make a good minimalist LPMud code base where open source contributors can start their LPMud not only with mudlibs, but also extensions to the LPMud Driver.

Please take a look into the following documents before you jump in:
- [Neolith LPMud Driver Internals](docs/manual/internals.md)
- [Neolith Developor Reference](docs/manual/dev.md)

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

