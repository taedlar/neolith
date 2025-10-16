# write_file()
Appends a string to a file

## SYNOPSIS
~~~
int write_file( string file, string str, int flag );
~~~

## DESCRIPTION
Append the string `str` into the file `file`. Returns 0 or 1 for failure or success.
If flag is 1, `write_file` overwrites instead of appending.

## SEE ALSO
[read_file()](read_file.md),
[write_buffer()](write_buffer.md),
[file_size()](file_size.md)
