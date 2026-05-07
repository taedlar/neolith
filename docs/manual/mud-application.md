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

## Usage
For console-mode applications, Neolith behaves like an LPC runtime hosted by the driver backend.
Even in console mode, the program runs as a real LPMud session with one interactive user.

### Quick start with `hello_openai.c`
The example at `examples/apps/hello_openai.c` sends the message `"hello"` to OpenAI with CURL efuns and parses the JSON response with JSON efuns.

1. Build Neolith with CURL and JSON support (`PACKAGE_CURL=ON`, `PACKAGE_JSON=ON`).
2. Add your API key in a local file beside the app:
   ```bash
   echo "sk-..." > examples/apps/.openai_api_key
   ```
3. Run in console mode:
   ```bash
   neolith -c examples/apps/hello_openai.c
   ```

Expected flow:
- `logon()` loads the API key and starts the request.
- `perform_using()` sets URL, headers, request body, and timeout.
- `perform_to()` dispatches a non-blocking HTTP transfer.
- The completion callback receives `success` + `buffer|string`, parses with `from_json()`, prints output, then calls `shutdown()`.

### Runtime characteristics
A MUD application keeps core LPMud semantics:
- **Application entry points**:
  - `epilog()` for optional preload logic
  - `connect()` to bind an incoming user to an object
  - `logon()` for per-user startup
- **Independent sessions**: Each user session is isolated; disconnect a user with `destruct()`.
- **Service lifecycle**: The backend loop continues until `shutdown()` is called.
- **Sandbox boundary**: File access remains restricted to the mudlib directory.
- **Piped I/O**: In console mode, standard input and output can be piped for automation.
- **Configurable deployment**: With `-f`, you can run the same app with production mudlib settings and open network ports.
- **Recoverable coding loop**: LPC compile/runtime errors do not crash the driver process; diagnostics are surfaced through standard error/log paths (including `log_error()` policy hooks).

## Advanced MUD application creation

Use this pattern when moving from one-shot scripts to production-style tools.

### 1) Separate bootstrap from business logic
- Keep master applies (`epilog()`, `connect()`, `logon()`) minimal.
- Delegate domain behavior to service objects and helper modules.
- In interactive apps, return a dedicated user object from `connect()` instead of reusing the master object.

### 2) Design for asynchronous operations
- Prefer non-blocking efuns (`perform_using()` + `perform_to()`) for network access.
- Treat callbacks as state transitions; store request context explicitly.
- Add timeout and retry policy for external APIs.

### 3) Use structured payload boundaries
- Build payloads as LPC mappings/arrays, serialize with `to_json()`.
- Parse external responses with `from_json()` before business logic.
- Validate expected schema (`mappingp`, `arrayp`, key existence) before indexing nested values.

### 4) Harden secrets and configuration
- Keep secrets out of source files; load from local files or deployment-managed secrets.
- Fail fast with clear operator-facing messages when required configuration is missing.
- When using `-f neolith.conf`, keep app paths inside `MudlibDir`.

### 5) Plan for multi-user and remote clients
- In LPC all variables are bound to an object; there are no global variables. Per-user state belongs on the user/session object, shared state belongs on a dedicated service object.
- Separate console automation flows from telnet/websocket user flows.
- Implement explicit teardown paths so abandoned sessions do not leak resources.
- **Live repair without restart**: When command-processing or other LPC code has bugs, the normal fix cycle is: edit the source file, `destruct()` the live object(s) carrying the old program, then let the driver recompile on the next access. No MUD restart is required. This is especially important for remote clients where downtime is disruptive.
  - Destructing a parent object does not affect clones that have already inherited its program; destruct those independently if needed.
  - Keep application logic in separate objects from the master file so bugs can be fixed without touching the master.

### 6) Test like a service
- Test console-mode behavior with scripted stdin/stdout.
- Add interaction tests for multi-user flows (for example with `examples/testbot.py`).
- Verify failure paths: network errors, invalid JSON, missing config, and callback object destruction.
