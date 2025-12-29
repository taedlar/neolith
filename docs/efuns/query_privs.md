# query_privs()
## NAME
**query_privs** - return the privs string for an object

## SYNOPSIS
~~~cxx
string query_privs( object ob );
~~~

## DESCRIPTION
Returns the privs string for an object.  The privs string is
determined at compile time via a call to privs_file() in the
master object, and changeable via the set_privs() efun.

This efun is only available if PRIVS is defined at driver
compile time.

## SEE ALSO
[privs_file()](privs_file.md), [set_privs()](set_privs.md)
