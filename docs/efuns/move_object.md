# move_object()
## NAME
**move_object** - move current object to another environment

## SYNOPSIS
~~~cxx
void move_object( object item, mixed dest );
~~~

## DESCRIPTION
Move the object `item' into the object `dest'.  **item** must
be this_object().  move_object may be optionally called with
one argument in which case **item** is implicitly
this_object() and the passed argument is **dest**.

## SEE ALSO
[this_object()](this_object.md), [move()](move.md)
