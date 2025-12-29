# allocate()
## NAME
**allocate** - allocate an array

## SYNOPSIS
~~~cxx
mixed *allocate( int size );
~~~

## DESCRIPTION
Allocate an array of **size** elements.  The number of
elements must be >= 0 and not bigger than a system maximum
(usually ~10000).  All elements are initialized to 0.

## SEE ALSO
[sizeof()](sizeof.md), [allocate_mapping()](allocate_mapping.md)
