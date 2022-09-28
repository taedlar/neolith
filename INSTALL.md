INSTALL
=======

# Setting Up Environment

The official development environment is Ubuntu 20.04 LTS.

For Windows users, the build is also tested with WSL (Windows Subsystem for Linux) on Windows 10.  
See [Install Linux on Windows with WSL](https://learn.microsoft.com/en-us/windows/wsl/install) for instructions to install Ubuntu on Windows.

You also need the following packages:  
```
$ sudo apt install build-essential
$ sudo apt install libtool
$ sudo apt install gettext
$ sudo apt install bison
```

# Build

For the first time you checkout the source code, run the script to setup autotools:  
```
$ ./bootstrap
```
To build the program (see `./configure --help` for more options):  
```
$ ./configure
$ make
```
If everything goes well, you should be able to find the executable file in `src/neolith`.

## Building standalone executable

The default settings of `libtool` builds shared libraries for smaller objects. In some cases you don't own the whole machine and wish to build a standalone binary and install it in your local directory. You can specify an install prefix and disable shared libraries with the configure script:  
```
$ ./configure --prefix=/home/mud/local --disable-shared
$ make install
```
This will build a standalone executable and install it to `/home/mud/local/bin/neolith`.
