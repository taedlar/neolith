# tail()
## NAME
**tail** - displays the latter portion of a file

## SYNOPSIS
~~~cxx
int tail( string path );
~~~

## DESCRIPTION
This efunction displays the latter portion of the file named
by path.  It returns 1 if successful, 0 otherwise (e.g. when
the file is protected against reading).

## SEE ALSO
[read_file()](read_file.md), [read_buffer()](read_buffer.md), [file_size()](file_size.md)
