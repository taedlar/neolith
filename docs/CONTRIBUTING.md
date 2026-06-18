Contributing
============

## Contribution Licensing Terms

By opening a pull request, submitting a patch, or otherwise contributing code,
tests, documentation, or other materials to this repository, you agree that:

- once merged, your contribution becomes part of this project and is licensed
	under the Fair-Code terms in the project LICENSE;
- Neolith does not impose additional non-commercial-use restrictions on code
	newly added or modified in this project by maintainers or contributors;
- Neolith remains a derived work of MudOS/LPMud, so upstream non-commercial
	restrictions can still limit commercial use of the combined work;
- the Neolith project and contributors do not assume responsibility to enforce
	or litigate upstream MudOS/LPMud licensing restrictions.

See LICENSE for the authoritative legal terms.

The following skills would be required if you are interested in contributing:
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
