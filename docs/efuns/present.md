# present()
## NAME
**present** - find an object by id

## SYNOPSIS
~~~cxx
object present( mixed str, object ob );
~~~

## DESCRIPTION
If an object that identifies to the name `str' is present,
then return it.

`str' can also be an object, in which case the test is much
faster and easier.

If `ob' is given, then the search is done in the inventory
of `ob', otherwise the object is searched for in the
inventory of the current object, and in the inventory of the
environment of the current object.

## SEE ALSO
[move_object()](move_object.md), [environment()](environment.md)
