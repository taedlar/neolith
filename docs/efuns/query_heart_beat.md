# query_heart_beat()
## NAME
**query_heart_beat** - query the status of an object's
heartbeat

## SYNOPSIS
~~~cxx
int query_heart_beat( object );
~~~

## DESCRIPTION
Returns the value with which set_heart_beat() has been
called with on `object'.  If object is not given, it
defaults to the current object.  If the object has no heart
beat, 0 will be returned.

## SEE ALSO
[heart_beat()](heart_beat.md), [set_heart_beat()](set_heart_beat.md)
