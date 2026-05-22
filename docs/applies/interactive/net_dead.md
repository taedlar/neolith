# net_dead()
## NAME
**net_dead** - called when an interactive object loses its connection

## SYNOPSIS
~~~cxx
void net_dead (void);
~~~

## DESCRIPTION
`net_dead()` is a disconnect callback for interactive objects.

Use it to handle cases where a player/client disappears unexpectedly, such as:

- network connection drop
- terminal/console session ending

From a mudlib point of view, this is the place to perform application-level
cleanup for a disconnected session.

Typical mudlib tasks in `net_dead()`:

- save player/application state
- clear temporary session state
- stop heartbeats or timers specific to that session
- notify rooms/channels or related game systems

Keep the handler short and robust. Avoid long-running logic and avoid relying
on additional input from the disconnected client.

If you need reconnection behavior, implement it in mudlib policy code (for
example, marking a session as disconnected and allowing later reattach).

In piped console mode, when stdin reaches EOF, `net_dead()` is called first.
Driver shutdown then proceeds after this apply returns.
