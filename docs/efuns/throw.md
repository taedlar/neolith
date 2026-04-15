# throw()
## NAME
**throw** - forces an error to occur in an object.

## SYNOPSIS
~~~cxx
void throw(mixed value);
~~~

## DESCRIPTION
The `throw()` efun forces immediate transfer to the nearest active
[catch()](catch.md) boundary.

When an active `catch()` exists, `throw(value)` returns `value` from `catch()`.
If `value` is `0`, the driver normalizes it to `"*Unspecified error"` so the
thrown path does not collide with the `catch()` success result (`0`).

Calling `throw()` without an active `catch()` raises the runtime error
`"*Throw with no catch."`.

Typical usage:

~~~cxx
string err;
int rc;

err = catch(rc = ob->move(dest));
if (err) {
    throw("move.c: ob->move(dest): " + err + "\n");
    return;
}
~~~

## SEE ALSO
[catch()](catch.md),
[error()](error.md),
[error_handler()](../applies/master/error_handler.md)
