# load_object
## NAME
load_object() - find or load an object by file name

## SYNOPSIS
~~~cxx
object load_object( string str );
~~~

## DESCRIPTION
Find the object with the file name *str*.
If the file exists and the object hasn't been loaded yet, it is loaded.
Otherwise zero is returned.

This is an alias of `find_object(str, 1)`.

## SEE ALSO
[file_name()](file_name.md),
[stat()](stat.md),
[find_object()](find_object.md)
