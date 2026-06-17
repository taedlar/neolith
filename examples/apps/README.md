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

**Run**:
```bash
OPENAI_API_KEY=sk-... /path/to/neolith -c hello_openai.c
```

The API key is read from the `OPENAI_API_KEY` environment variable. Two additional variables are optional:

| Variable | Default | Description |
|---|---|---|
| `OPENAI_API_KEY` | *(required)* | API key |
| `OPENAI_BASE_URL` | `https://api.openai.com/v1` | Base URL — override for local or compatible endpoints |
| `OPENAI_MODEL` | `gpt-4o` | Model name |

Example using a local [Ollama](https://ollama.com) endpoint:
```bash
OPENAI_API_KEY=x \
OPENAI_BASE_URL=http://localhost:11434/v1 \
OPENAI_MODEL=llama3 \
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
