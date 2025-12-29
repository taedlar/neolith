# file_size()
## NAME
**file_size** - get the size of a file

## SYNOPSIS
~~~cxx
int file_size( string file );
~~~

## DESCRIPTION
file_size() returns the size of file **file** in bytes.  Size
-1 indicates that **file** either does not exist, or that it
is not readable by you. Size -2 indicates that **file** is a
directory.

## SEE ALSO
[stat()](stat.md), [get_dir()](get_dir.md)
