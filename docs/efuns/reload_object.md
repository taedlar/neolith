# reload_object
## NAME
reload_object - return an object to its just-loaded state

## SYNOPSIS
~~~cxx
void reload_object (object ob);
~~~

## DESCRIPTION
When reload_object() is called on *ob*, all the driver-maintained properties are re-initialized (heart_beat, call_outs, light, shadows, etc), all variables are re-initialized, and create() is called.
It has a similar effect to destructing/reloading the object, however, no disk access or parsing is performed.

## SEE ALSO
[export_uid()](export_uid.md),
[new()](new.md),
[clone_object()](clone_object.md),
[destruct()](destruct.md)
