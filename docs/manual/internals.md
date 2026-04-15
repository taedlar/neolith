Neolith LPMud Driver Internals
==============================

> :pushpin: This article describes Neolith-specific behavior, which differs from original LPMud and MudOS.

# Starting Up

When the LPMud Driver process starts, it goes through the following steps before it begins accepting connections:

1. Process command line arguments
2. Process configuration file
3. Initialize LPC compiler
4. Initialize LPC virtual machine
5. Load simul efun object (optional)
6. Load master object
7. Do epilogue

# Backend Loop

After startup, the LPMud Driver enters an infinite backend loop that **accepts user connections** while running housekeeping tasks in the background.

> [!Note]
> If console mode is enabled by specifying `-c` in the Neolith command line, the [console user](console-mode.md) is connected first before entering the loop. 

In backend mode, the LPMud Driver:
- Accepts connections on the TCP port specified in the configuration file.
- Compiles and loads additional objects when required by LPC code.
- Animates objects by calling `heart_beat()` for objects with heartbeats enabled.
- Calls scheduled functions installed by `call_out()`.
- Renews the world by periodically calling `reset()` on objects.
- Frees unused objects by calling `clean_up()` on objects that have been idle.

The LPMud Driver stays in backend mode until `shutdown()` is called or the process is otherwise terminated.

# LPC Objects

> Everything in a LPMud world is an **LPC object**, created from an LPC program file. Internally, an LPC object is identified by its
> filename (relative to the mudlib directory).

When the LPMud Driver is running the backend loop, at least one LPC object exists: the master object.
Depending on mudlib design, additional objects may be loaded in `epilog()` before the driver is ready to accept user connections.

# Error Model (Mudlib-Facing)

Neolith now uses C++ exception transport internally for runtime error handling.
For mudlib code, the observable LPC behavior remains contract-first:

- `catch(expr)` returns `0` on success.
- Driver/runtime errors seen through `catch()` are typically `*`-prefixed strings.
- `throw(value)` returns `value` through the nearest active `catch()`.
- `throw(0)` is normalized to `"*Unspecified error"` so it does not collide with
  the `catch()` success value (`0`).
- Calling `throw()` without an active `catch()` raises `"*Throw with no catch."`.

Master applies involved in runtime error routing:

- Uncaught runtime errors call `master::error_handler(error)`.
- Caught-path callback `master::error_handler(error, 1)` is build-option
  dependent (`LOG_CATCHES`).
- Fatal driver shutdown path calls `master::crash(msg, command_giver,
  current_object)` before termination.

The migration retired production `setjmp`/`longjmp` transport paths in favor of
typed exception routing at driver boundaries, while preserving LPC-level
behavior contracts expected by mudlibs.

For implementation details, see
[Driver Error Routing Internals](../internals/error-runtime-routing.md).
