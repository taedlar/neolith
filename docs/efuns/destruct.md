# destruct()
## NAME
**destruct** - remove an object from the games

## SYNOPSIS
~~~cxx
void destruct( object ob );
~~~

## DESCRIPTION
Completely destroy and remove object `ob'. After the call to
destruct(), no global variables will exist any longer, only
locals, and arguments.  If `ob' is this_object(), execution
will continue, but it is best to return a value immediately.

## SEE ALSO
[clone_object()](clone_object.md), [new()](new.md), [destruct_env_of()](destruct_env_of.md), [move()](move.md)
