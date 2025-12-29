# children()
## NAME
**children** - returns an array of objects cloned from a given
object.

## SYNOPSIS
~~~cxx
object *children( string name );
~~~

## DESCRIPTION
This efunction returns an array of objects that have been
cloned from the file named by **name**, as well as the object
**name** itself, if loaded.  An example use of children() is
to find all objects that have been cloned from the user
object:

object *list;

list = children("/obj/user");

This lets you find all users (both netdead and interactive
whereas users() only reports interactive users).

## SEE ALSO
[deep_inherit_list()](deep_inherit_list.md), [inherit_list()](inherit_list.md), [objects()](objects.md)
