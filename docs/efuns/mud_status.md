# mud_status()
## NAME
**mud_status** - report various driver and mudlib statistics

## SYNOPSIS
~~~cxx
void mud_status( int extra );
~~~

## DESCRIPTION
This function writes driver and mudlib statistics to the
caller's screen.  If extra is non-zero, then additional
information will be written.  This function replaces the
hardcoded **status** and 'status tables' commands in vanilla
3.1.2.

## SEE ALSO
[debug_info()](debug_info.md), [dumpallobj()](dumpallobj.md), [memory_info()](memory_info.md), [uptime()](uptime.md)
