# valid_bind()
## NAME
**valid_bind** - controls the use of the bind() efun

## SYNOPSIS
~~~cxx
int valid_bind (object binder, object old_owner, object new_owner);
~~~

## DESCRIPTION
This routine is called when **binder** attempts to use the [bind()](../../efuns/bind.md) efun to make a function pointer which belongs to **old_owner** belong to **new_owner** instead.
If this function returns 0, an error occurs.

## SEE ALSO
[bind()](../../efuns/bind.md)
