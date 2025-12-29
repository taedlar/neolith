# set_privs()
## NAME
**set_privs** - set the privs string for an object

## SYNOPSIS
~~~cxx
void set_privs( object ob, string privs );
~~~

## DESCRIPTION
Sets the privs string for `ob' to `privs'.

This efun is only available if PRIVS is defined at driver
compile time.

## SEE ALSO
[privs_file()](privs_file.md), [query_privs()](query_privs.md)
