Neolith LPMud Driver
====================

## License
Neolith is a LPMud Driver that run the [Eastern Stories 2 MUD](https://zh.wikipedia.org/wiki/%E6%9D%B1%E6%96%B9%E6%95%85%E4%BA%8B2_%E5%A4%A9%E6%9C%9D%E5%B8%9D%E5%9C%8B) (up since 1995). The code was modified from MudOS v22pre5, which is a derived work of the original LPMud by Lars Pensjö. I intended to distribute my parts of code in GPL style, and the copyright notices from original authors of MudOS and LPMud should also apply to this derived work (see [Copyright](doc/Copyright) for details).

Although the GPLv2 [LICENSE](LICENSE) allows "commercial use", this program contains additional restrictions from original authors. In brief:  
- **"May not be used in any way whatsoever for monetary gain"** (restriction by Lars Pensjö)
- **GPLv2** (open source required, must comply all restrictions from all authors)

So, the conclusion is commercial-use NOT allowed.

## Install
Neolith is mainly written in C Language. For building the program, see [INSTALL](INSTALL.md).

## Using Neolith
To start a MUD using Neolith, you need a Neolith configuration file. See [neolith.conf](src/neolith.conf) for an example. You can launch the MUD by the command:
```
$ neolith -f <path-to-neolith.conf> &
```
