# new()
## NAME
**new** - load a copy of an object

## SYNOPSIS
~~~cxx
object new( string name );
~~~

## DESCRIPTION
Load a new object from definition **name**, and give it a new
unique name.  Returns the new object.  An object with a
nonzero environment() cannot be cloned.

## SEE ALSO
[clone_object()](clone_object.md), [destruct()](destruct.md), [move_object()](move_object.md)
