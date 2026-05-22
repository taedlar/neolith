# MUD Application Examples

A [MUD application](../../docs/manual/mud-application.md) is a single LPC file passed as the first non-option argument to `neolith`. The driver compiles and runs it directly, making it a lightweight way to prototype or demonstrate driver features without setting up a full mudlib.

## Examples

### hello_world.c

Prints a message to the console and exits. Good starting point for verifying a working driver build.

```bash
/path/to/neolith -c hello_world.c
```

Expected output:
```
Hello World from LPC!
```

### hello_openai.c

Sends a "hello" message to the OpenAI Chat Completions API and prints the reply. Demonstrates async HTTP via `PACKAGE_CURL` and JSON parsing via `PACKAGE_JSON`.

**Requirements**: driver built with `PACKAGE_CURL=ON` and `PACKAGE_JSON=ON` (both are on by default in dev builds).

**Setup**: place your OpenAI API key in a file named `.openai_api_key` in this directory:

```bash
echo "sk-..." > examples/apps/.openai_api_key
```

**Run**:
```bash
/path/to/neolith -c hello_openai.c
```

## Writing Your Own Application

A minimal MUD application implements two entry points:

```c
// Return the object that will handle the connection (or this_object() for
// single-user / console mode).
object connect(int port) {
    return this_object();
}

// Called after connect(); put your application logic here.
void logon() {
    write("Hello!\n");
    shutdown();
}
```

Run it in console mode with `-c`:

```bash
/path/to/neolith -c my_app.c
```

See [Console Mode](../../docs/manual/console-mode.md) for details on `-c` and piped I/O.
