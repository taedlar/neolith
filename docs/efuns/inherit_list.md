# inherit_list()
## NAME
**inherit_list** - get a list of parents of an object

## SYNOPSIS
~~~cxx
string *inherit_list( object obj );
~~~

## DESCRIPTION
Returns an array of filenames of objects inherited by obj.
Only directly inherited files are returned.  E.g. if A
inherits B which inherits C, inherit_list(A) will return an
array with B, but not C.

## SEE ALSO
[deep_inherit_list()](deep_inherit_list.md), [inherits()](inherits.md)
