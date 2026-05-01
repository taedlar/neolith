MUD Application
=====

In addition to running as a MUD server, you can create general-purpose **applications in LPC** and execute the program via `neolith` command.

The usage is simple:
```bash
neolith [options] <lpc-file>
```

## Architecture
Traditionally, a MudOS LPMud driver requires a configuration file that supplies settings for `mudlib directory`, `master file`, ... etc. when starts up.
If you take a look at the core of a LPMud, there are only three essential components required:
- A **mudlib** directory that defines the filesystem boundary the LPMud code can access.
- A **master** object that is the source of authorizations (policies) when LPC programs interact with each other and the filesystem or network.
- Settings for incoming **port** from which user can enter the MUD.

Neolith has enhanced the LPMud driver starting-up path and make the configuration file *optional*.
This allows standalone LPC programs or **MUD applications** to be created.

### Single-file MUD application
For a minimal viable MUD application:
- Single LPC file and implicit mudlib directory
- No configuration file (with default settings)
- Run with [console mode](console-mode.md).

Example:
```cxx
// hello_world.c - a minimal MUD application
object connect(int port) {
    return this_object();
}

void logon() {
    write ("Hello World!\n");
    shutdown();
}
```

To run the MUD application:
```bash
neolith -c hello_world.c
```

The example prints the message to console and exits.

- The file `hello_world.c` is used as the **master file** to create a minimal LPMud that does not listen on any TCP port.
- Without specifying a configuration file with `-f` option, Neolith uses the **parent directory** of `hello_world.c` as the mudlib directory.
- With `-c` option, the driver connects the **console user** on start up.
- In the master apply [`connect`](/docs/applies/master/connect.md), the master object itself is returned as the **user object** for the console user.
- The driver calls [`logon`](/docs/applies/interactive/logon.md) apply on the user object, which is master object itself. The LPC code prints the message to console user and shutdown the MUD.

### Regular MUD application
With configuration file (`neolith.conf`), a MUD application is defined by specifying a particular **master file** via the driver command line argument (overriding the `MasterFile` setting in the configuration file).

For example:
```bash
neolith -f neolith.conf -c mudlib/adm/apps/migrate_player_file.c
```
> [!NOTE]: When configuration file is specified, the mudlib directory must be explicitly define with `MudlibDir`.
> Specifying a MUD application outside the mudlib directory will be rejected.

A regular MUD application shares the same configuration settings with the production MUD server (e.g. simul efuns), while starting with its own [`epilog()`](/docs/applies/master/epilog.md) stage, its own [`connect()`](/docs/applies/master/connect.md) interface, and all the policies controlled by master file.

Example of regular MUD application use cases:
- Experiment mass mudlib refactoring
- Sandboxing access to the MUD for agentic AI users
- Running as MCP server for AI coding agents
- Exercise maintenance tasks with tailor-made master file in console mode
- Automate LPC code testings or performance testings.

### Packaged MUD application (planned)
If the MUD application is an archive, it is used as a packaged (read-only) mudlib serving as MUD applications.
- When used alone, the archive provides a file tree acting as the mudlib directory. The driver shall look for a `config.json` file in the archive for labelled configuration settings. For example:
  ```json
  {
    ".defaults": {
        "SimulEfunFile": "/adm/obj/simul_efun.c"
    },
    "production": {
        "inherits": [".defaults"],
        "MasterFile": "/adm/obj/master.c",
        "Port": [[4000, "telnet"]]
    },
    "migrate-player-file": {
        "inherits": [".defaults"],
        "MasterFile": "/adm/apps/migrate_player_file.c"
    }
  }
  ```
  - Each label name a MUD application in the package.
  - If the label name start with a dot(`.`), it is hidden from the UI.
- When used with a configuration file, it overrides the `MudlibDir` setting and restricts the driver to load LPC files only from the archive.
  - Settings in the configuration file overrides `config.json`.

To launch particular MUD application in an archive, add the label after archive name. For example:
```bash
neolith -c package.zip migrate-player-file
```
