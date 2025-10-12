INSTALL
=======

# Setting Up Build Environment

The recommended development environment is Ubuntu 20.04 LTS or later.

For Windows users, the build is also tested with WSL (Windows Subsystem for Linux) on Windows 10.  
See [Install Linux on Windows with WSL](https://learn.microsoft.com/en-us/windows/wsl/install) for instructions to install Ubuntu on Windows.

You also need the following packages:
~~~sh
sudo apt install build-essential
sudo apt install libtool
sudo apt install gettext
sudo apt install bison
~~~

# Building with autotools

For the first time you checkout the source code, run the script to setup autotools:  
~~~sh
./bootstrap
~~~
To build the program (see `./configure --help` for more options), create a build directory and build from it:
~~~sh
mkdir build
cd build
../configure
make
~~~
If everything goes well, you should be able to find the executable file `build/src/neolith`.

## Standalone executable

The default settings of `libtool` builds shared libraries for smaller objects.
In some cases you don't own the whole machine and wish to build a standalone binary and install it in your local directory.
You can specify an install prefix and disable shared libraries with the configure script:  
~~~sh
configure --prefix=/home/mud/local --disable-shared
make install
~~~
This will build a standalone executable and install it to `/home/mud/local/bin/neolith`.

# Building with CMake

Building Neolith with CMake is made simple via presets:
~~~sh
cmake --preset linux
cmake --build --preset ci-linux
~~~
The executable can be found in `out/build/linux/src/RelWithDebInfo/neolith`.
