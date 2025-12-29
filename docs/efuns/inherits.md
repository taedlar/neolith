# inherits()
## NAME
**inherits** - determine if an object inherits a given file

## SYNOPSIS
~~~cxx
int inherits( string file, object obj );
~~~

## DESCRIPTION
inherits() returns 0 if obj does not inherit file, 1 if it
inherits the most recent copy of file, and 2 if it inherits
an old copy of file.

## SEE ALSO
[deep_inherit_list()](deep_inherit_list.md), [inherit_list()](inherit_list.md)
