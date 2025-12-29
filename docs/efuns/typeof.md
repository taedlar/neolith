# typeof()
## NAME
**typeof** - return the type of an expression

## SYNOPSIS
~~~cxx
int typeof( mixed var );
~~~

## DESCRIPTION
Return the type of an expression.  The return values are
given in <typeof.h>.  They are:

INT             2 STRING    4 ARRAY     8 OBJECT    16
MAPPING         32 FUNCTION        64 FLOAT           128
BUFFER          256

## SEE ALSO
[allocate()](allocate.md), [allocate_mapping()](allocate_mapping.md), [strlen()](strlen.md)
