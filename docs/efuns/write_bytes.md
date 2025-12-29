# write_bytes()
## NAME
**write_bytes** - writes a contiguous series of bytes to a
file

## SYNOPSIS
~~~cxx
int write_bytes( string path, int start, string series );
~~~

## DESCRIPTION
This function writes the bytes in **series** into the file
named by **path** beginning at byte # **start**.  It returns
zero (0) upon failure, 1 otherwise.

## SEE ALSO
[write_file()](write_file.md), [read_bytes()](read_bytes.md)
