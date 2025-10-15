INSTALL
=======

# Setting Up Build Environment

The recommended development environment is Ubuntu 20.04 LTS or later.

For Windows users, the build is also tested with WSL (Windows Subsystem for Linux) on Windows 10.  
See [Install Linux on Windows with WSL](https://learn.microsoft.com/en-us/windows/wsl/install) for instructions to install Ubuntu on Windows.

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
The executable can be found in `out/build/linux/src/RelWithDebInfo/neolith`.
