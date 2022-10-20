Neolith LPMud Driver
====================
[![CodeQL](https://github.com/taedlar/neolith/workflows/CodeQL/badge.svg)](https://github.com/taedlar/neolith/actions?query=workflow%3ACodeQL)

## Main Features
- UTF-8 support (tons of fixes on top of MudOS code)
- Integrated with GNU autotools.
- Refactored driver debug log / tracing messages.
- Refactored usage of obsoleted C library dependency.
- Current official development platform: Ubuntu 20.04 (Native & WSL2)

## License
Neolith is a LPMud Driver that run the [Eastern Stories 2 MUD](https://zh.wikipedia.org/wiki/%E6%9D%B1%E6%96%B9%E6%95%85%E4%BA%8B2_%E5%A4%A9%E6%9C%9D%E5%B8%9D%E5%9C%8B) (up since 1995). The code was modified from MudOS v22pre5, which is a derived work of the original LPMud by Lars Pensjö. I intended to distribute my parts of code in GPL style, and the copyright notices from original authors of MudOS and LPMud should also apply to this derived work (see [Copyright](docs/Copyright) for details).

Although the GPLv2 [LICENSE](LICENSE) allows "commercial use", this program contains additional restrictions from original authors. In brief:  
- **"May not be used in any way whatsoever for monetary gain"** (restriction by Lars Pensjö)
- **GPLv2** (open source required, must comply all restrictions from all authors)

So, the conclusion is commercial-use NOT allowed.

## Install
Neolith is distributed in source code and mainly written in C Language. You need to build the executables from source code before you can install it.

See [INSTALL](INSTALL.md) for detailed instructions.

## Usage
To start a MUD using Neolith, you need a Neolith configuration file.
Please make a copy from [src/neolith.conf](src/neolith.conf) and customize the settings for your needs.
You can launch the MUD by the command:
```
$ neolith -f <path-to-neolith.conf> &
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
