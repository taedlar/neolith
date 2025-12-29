# allocate_buffer()
## NAME
**allocate_buffer** - allocate a buffer

## SYNOPSIS
~~~cxx
buffer allocate_buffer( int size );
~~~

## DESCRIPTION
Allocate a buffer of **size** elements.  The number of
elements must be >= 0 and not bigger than a system maximum
(usually ~10000).  All elements are initialized to 0.

## SEE ALSO
[bufferp()](bufferp.md), [read_buffer()](read_buffer.md), [write_buffer()](write_buffer.md), [sizeof()](sizeof.md),
[to_int()](to_int.md)

## AUTHOR
Truilkan
