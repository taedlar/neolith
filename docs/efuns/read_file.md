# read_file()
## NAME
**read_file** - read a file into a string

## SYNOPSIS
~~~cxx
string read_file( string file, int start_line,
int number_of_lines );
~~~

## DESCRIPTION

Reads lines of text from a file into a string. The second and third arguments are optional. If only the first argument is specified, the entire file is returned as a string.

**NUL character handling:**
`read_file()` no longer rejects or truncates at embedded NUL (\0) bytes. The returned string may contain any byte value, including NULs, and will preserve the file's exact contents for the selected lines.

**Legacy C string compatibility:**
If you require a string truncated at the first NUL byte (for compatibility with APIs or efuns expecting C-string semantics), use the [`c_str()`](c_str.md) efun on the result.

The `start_line` is the line number of the line you wish to read. This routine will return 0 if you try to read past the end of the file, or if you try to read from a nonpositive line.

## SEE ALSO
[write_file()](write_file.md), [read_buffer()](read_buffer.md), [c_str()](c_str.md)
