# object_name()
## NAME
**object_name** - called by the driver to find out an object's name

## SYNOPSIS
~~~cxx
string object_name (object ob);
~~~

## DESCRIPTION
This master apply is called by the [sprintf()](../../efuns/sprintf.md) efun, when printing the "value" of an object.
This function should return a string corresponding to the name of the object (eg a user's name).

## SEE ALSO
[file_name()](../../efuns/file_name.md)
