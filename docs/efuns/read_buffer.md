# read_buffer()
## NAME
**read_buffer** - read from a file and return a buffer, or
return part of a buffer as a string

## SYNOPSIS
~~~cxx
string | buffer read_buffer( string | buffer src,
int start,  int len );
~~~

## DESCRIPTION
If `src' is a string (filename), then the filename will be
read, starting at byte # `start', for `len' bytes, and
returned as a buffer.  If neither argument is given, the
entire file is read.

If `src' is a buffer, then characters are read from the
buffer beginning at byte # `start' in the buffer, and for
`len' # of bytes, and returned as a string.

Note that the maximum number of bytes you can read from a
file and into a buffer is controlled via the 'maximum byte
transfer' parameter in the runtime config file.

## SEE ALSO
[write_buffer()](write_buffer.md), [allocate_buffer()](allocate_buffer.md), [bufferp()](bufferp.md),
[read_bytes()](read_bytes.md), [write_bytes()](write_bytes.md)

## AUTHOR
Truilkan
