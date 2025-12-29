# refs()
## NAME
**refs** - return the number of references to a data structure

## SYNOPSIS
~~~cxx
int refs( mixed data );
~~~

## DESCRIPTION
The number of references to `data' will be returned by
refs().  This is useful for deciding whether or not to make
a copy of a data structure before returning it.

## SEE ALSO
[children()](children.md), [inherit_list()](inherit_list.md), [deep_inherit_list()](deep_inherit_list.md),
[objects()](objects.md)
