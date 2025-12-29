# virtualp()
## NAME
**virtualp** - determine whether a given variable points to a virtual object

## SYNOPSIS
~~~cxx
int virtualp (object arg);
~~~

## DESCRIPTION
Returns true (`1`) if the argument is objectp() and the O_VIRTUAL flag is set.
The driver sets the O_VIRTUAL flag for those objects created via the

## SEE ALSO
[clonep()](clonep.md), [userp()](userp.md), [wizardp()](wizardp.md), [objectp()](objectp.md), [new()](new.md), [clone_object()](clone_object.md), [call_other()](call_other.md), [file_name()](file_name.md)
