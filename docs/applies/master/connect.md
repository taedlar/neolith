# connect()
## NAME
**connect** - get an object for a new user

## SYNOPSIS
~~~cxx
object connect (int port);
~~~

## DESCRIPTION
The driver calls `connect()` in the master object whenever a new user connects to the driver from `port`.
The object returned by `connect()` is used as the initial user object.

In Neolith, if `port` is zero, the user is connected as a console user.

Note that it is possible to use [exec()](../../efuns/exec.md) to switch the user connection from the initial user object to some other object.

**Single-user mode** is allowed in Neolith by returning `this_object()` from the `connect()` (warning: this causes a leaked connection in MudOS).
When in single-user mode, the master object is occupied by exactly one connected user and the driver will reject any incoming connections until the master object is available again.

## SEE ALSO
[exec()](../../efuns/exec.md),
[logon()](../interactive/logon.md)
