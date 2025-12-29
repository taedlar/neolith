# clone_object()
## NAME
**clone_object** - load a copy of an object

## SYNOPSIS
~~~cxx
object clone_object( string name );
~~~

## DESCRIPTION
Load a new object from definition **name**, and give it a new
unique name.  Returns the new object.  An object with a
nonzero environment() cannot be cloned.

## SEE ALSO
[destruct()](destruct.md), [move_object()](move_object.md), [new()](new.md)
