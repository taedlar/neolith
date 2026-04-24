# command()
## NAME
**command** - execute a command as if given by the object

## SYNOPSIS
~~~cxx
int command (string str, object ob);
~~~

## DESCRIPTION
Execute `str` for object `ob`, or this_object() if `ob` is
omitted.  Note that the usability of the second argument is
determined by the local administrator and will usually not
be available, in which case an error will result.  In case
of failure, 0 is returned, otherwise a numeric value is
returned, which is the LPC "evaluation cost" of the command.
Bigger numbers mean higher cost, but the whole scale is
subjective and unreliable.

When `ob` is provided, the driver applies additional protection
rules:

- If `str` is too long for the internal command buffer, an error
	is raised.
- If `ob` is interactive and not equal to this_object(), an error
	is raised.
- If `ob` is destructed, `command()` returns 0.
- Non-interactive objects (e.g. NPC) may still be used as `ob` even when they
	are not this_object().

These rules are enforced by the internal `command_for_object()`
helper to prevent unsafe command forcing of other interactive users.

## SEE ALSO
[add_action()](add_action.md), [enable_commands()](enable_commands.md)
