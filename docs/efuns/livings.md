# livings()
## NAME
**livings** - return an array of all living objects

## SYNOPSIS
~~~cxx
object *livings( void );
~~~

## DESCRIPTION
Returns an array of pointers to all living objects (objects
that have had enable_commands() called in them).

## SEE ALSO
[enable_commands()](enable_commands.md), [find_living()](find_living.md), [users()](users.md), [objects()](objects.md)
