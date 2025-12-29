# find_object()
## NAME
**find_object** - find an object by file name

## SYNOPSIS
~~~cxx
object find_object( string str, int load );
~~~

## DESCRIPTION
Find the object with the file name **str**.
If the object is a cloned object, then it can be found using the file name which would by returned if file_name() was called with it as the argument.

If the object was not found, and the optional second argument is non-zero (default: 0), the object is loaded and returned.

## SEE ALSO
[file_name()](file_name.md),
[stat()](stat.md)
