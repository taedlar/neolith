Contributing
============

The following skills would be required if you are intereseted in contributing:
- C/C++ programming
- CMake scripting
- LPMud administration (being wizard) and LPC programming
- Python programming (for writing test scripts)

To start contributing, add your self-introduction in the Discussion page and mention [taedlar](https://github.com/taedlar).

### Setting up `hatch` environment for testing

By default the `hatch` command looks for `pyproject.toml` from current working directory and parent directories.
You may add the `examples/m3_testbots` directory to `hatch` configuration file and enable project mode to allow running testbots without changing directory to the testbots directory.

```bash
# Add m3_testbots to hatch projects
hatch config set projects.m3_testbots /path/to/m3_testbots

# Enable hatch project mode
hatch config set mode project
```
