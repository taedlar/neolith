# clonep()
## NAME
**clonep** - determine whether or not a given variable points to a cloned object

## SYNOPSIS
~~~cxx
int clonep (void | mixed arg);
~~~

## DESCRIPTION
Returns true (1) iff the argument is objectp() and the O_CLONE flag is set.
The driver sets the O_CLONE flag for those objects created via [new()](new.md) ([clone_object()](clone_object.md)).
The clonep() efun will not return true when called on objects that are the master copy (those that are loaded via [call_other()](call_other.md)).
Note that if clonep() returns true, then file_name() will return a string containing a '#'.

clonep() defaults to this_object().

## SEE ALSO
[virtualp()](virtualp.md), [userp()](userp.md), [wizardp()](wizardp.md), [objectp()](objectp.md), [new()](new.md), [clone_object()](clone_object.md), [call_other()](call_other.md), [file_name()](file_name.md)
