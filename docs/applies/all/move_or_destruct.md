# move_or_destruct()
## NAME
**move_or_destruct** - ask an object to move to the specified destination

## SYNOPSIS
~~~cxx
int move_or_destruct (object dest);
~~~

## DESCRIPTION
If an object's environment is destructed, this apply is called on its contents.
**dest** will be the environment of the destructing object, or zero if it has none.
If the object does not move itself out of the object being destructed, it will be destructed as well.

## SEE ALSO
[destruct()](../../efuns/destruct.md),
[move_object()](../../efuns/move_object.md)
