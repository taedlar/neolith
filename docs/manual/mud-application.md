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
- Run with [console mode](coosole-mode.md).

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
- In the master apply [`connect`](/docs/applies/master/connect.md), the master obejct itself is returned as the **user object** for the console user.
- The driver calls [`logon`](/docs/applies/interactive/logon.md) apply on the user object, which is master object itself. The LPC code prints the message to console user and shutdown the MUD.
