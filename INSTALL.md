INSTALL
=======

# Setting Up Environment

- Ubuntu 20.04
- For Windows users, the build is also tested with WSL (Windows Subsystem for Linux) on Windows 10.   
  See https://learn.microsoft.com/en-us/windows/wsl/install for instructions to install Ubuntu on Windows.
- You also need the following packages:  
```
$ sudo apt install build-essential
$ sudo apt install libtool
$ sudo apt install gettext
$ sudo apt install bison
```

# Build

- For the first time you checkout the source code, run the script to setup autotools:  
```
$ ./bootstrap
```
- To build the program (see `./configure --help` for options):  
```
$ ./configure
$ make
```
- If everything goes well, you should be able to find the executable file in `src/neolith`.


