# load_object()
## NAME
**load_object** - find or load an object by file name

## SYNOPSIS
~~~cxx
object load_object( string str );
~~~

## DESCRIPTION
Find the object with the file name **str**.
If the file exists and the object hasn't been loaded yet, it is loaded.
Otherwise zero is returned.

When **str** does not identify an already loaded object, the driver canonicalizes
the object name and checks for an LPC source file with the `.lpc` extension first.
If that file does not exist, it falls back to the legacy `.c` extension. For
example, `load_object("room/start")` checks `room/start.lpc` and then
`room/start.c`.

The `.c` suffix is treated as a source extension during canonicalization, so
`load_object("room/start.c")` checks `room/start.lpc` before `room/start.c` as
well. It does not check `room/start.c.lpc`. The resulting object name omits the
source extension.

This is an alias of `find_object(str, 1)`.

## SEE ALSO
[file_name()](file_name.md),
[stat()](stat.md),
[find_object()](find_object.md)
