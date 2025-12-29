# enable_commands()
## NAME
**enable_commands** - allow object to use 'player' commands

## SYNOPSIS
~~~cxx
void enable_commands( void );
~~~

## DESCRIPTION
enable_commands() marks this_object() as a living object,
and allows it to use commands added with add_action() (by
using command()).  When enable_commands() is called, the
driver also looks for the local function catch_tell(), and
if found, it will call it every time a message (via say()
for example) is given to the object.

## BUGS
Do not call this function in any other place than create()
or strange things will likely occur.

## SEE ALSO
[this_object()](this_object.md), [living()](living.md), [add_action()](add_action.md), [command()](command.md),
[catch_tell()](catch_tell.md), [say()](say.md), [create()](create.md)
