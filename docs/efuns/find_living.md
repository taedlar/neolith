# find_living()
## NAME
**find_living** - find a living object matching a given id

## SYNOPSIS
~~~cxx
object find_living( string str );
~~~

## DESCRIPTION
Find first the object that is marked as living, and answers to the id `str`.
A living object is an object that has called `enable_commands()`.
The object must have set a name with `set_living_name()`, so its name will be entered into the hash table used to speed up the search for living objects.

## SEE ALSO
[living()](living.md),
[livings()](livings.md),
[find_player()](find_player.md),
[users()](users.md),
[enable_commands()](enable_commands.md),
[set_living_name()](set_living_name.md)
