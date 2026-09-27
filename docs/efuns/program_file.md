# program_file()
## NAME
**program_file** - get the name of the program compiled for an object

## SYNOPSIS
```c
mixed program_file( object ob, int all );
```

## DESCRIPTION
Returns the name of the program compiled for **ob**, with a leading `/`.
Unlike [file_name()](file_name.md) / [otable_key()](file_name.md), this value
reflects the actual source file used to compile the program, including its
`.lpc` or `.c` extension.

**ob** may be passed explicitly (`program_file(ob)`) or via dot-call syntax
(`ob.program_file()`); there is no default for **ob**.

If **all** is zero (the default), `program_file()` returns a single string:
the program's own file name.

If **all** is non-zero, `program_file()` returns an array of strings. The
first element is the program's own file name, followed by the file name of
every file that was `#include`d while compiling the program.

## SEE ALSO
[file_name()](file_name.md), [inherit_list()](inherit_list.md), [deep_inherit_list()](deep_inherit_list.md)
