# cp()
## NAME
**cp** - copy a file

## SYNOPSIS
~~~cxx
int cp( string src, string dst );
~~~

## DESCRIPTION
Copies the file **src** to the file **dst**.

## RETURN VALUE
Returns `1` for success.

Returns a negative value on failure, with the current implementation mapping:

- `-1`: source path validation failed, source open failed, or source read failed.
- `-2`: destination path validation failed, destination open failed, or destination path construction failed.
- `-3`: write/read failure during copy loop.

The specific negative values are implementation details and may change in future.
Mudlib code should treat any negative return value as failure.

## SEE ALSO
[rm()](rm.md), [rmdir()](rmdir.md), [rename()](rename.md), [link()](link.md)
