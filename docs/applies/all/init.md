# init()
## NAME
**init** - function in an object called by move_object() to initialize verb/actions

## SYNOPSIS
~~~cxx
void init (void);
~~~

## DESCRIPTION
When the mudlib moves an object "A" inside another object "B", the driver (the [move_object()](../../efuns/move_object.md) efun) does the following:

1. if "A" is living, causes "A" to call the `init()` in "B"

2. causes each living object in the inventory of "B" to call `init()` in "A", regardless of whether "A" is living or not.

3. if "A" is living, causes "A" to call the `init()` in each object in the inventory of "B".

> [!NOTE]
> An object is considered to be living if [enable_commands()](../../efuns/enable_commands.md) has been called by that object.

Typically, the `init()` function in an object is used to call [add_action()](../../efuns/add_action.md) for each command that the object offers.

## SEE ALSO
[reset()](reset.md),
[move_object()](../../efuns/move_object.md),
[enable_commands()](../../efuns/enable_commands.md),
[living()](../../efuns/living.md),
[add_action()](../../efuns/add_action.md)
