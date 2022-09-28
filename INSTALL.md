Installing Neolith
==================

=Development environment=

- Ubuntu 20.04, Using WSL (Windows Subsystem for Linux) on Windows 10 
  See https://learn.microsoft.com/en-us/windows/wsl/install for instructions to setup WSL
- For compile, install the following packages:
	$ sudo apt install build-essential
	$ sudo apt install libtool
	$ sudo apt install gettext
	$ sudo apt install bison


=Build=

- For the first time you checkout the source code from git, run the script to setuo autotools:
	$ ./bootstrap

- The building process is typical:
	$ ./configure
	$ make


