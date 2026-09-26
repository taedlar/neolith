# file_name / otable_key
## NAME
`file_name` - legacy alias for `otable_key`, which gets the object-table key

## SYNOPSIS
```c
string file_name (object ob);
string otable_key (object ob);
```

## DESCRIPTION

`file_name` and `otable_key` return the object's object-table key. The value
is the canonical object name, with a leading `/`, and does not include the
source file extension. `file_name` is retained as a legacy alias.

If the object is a cloned object, then `file_name` will not be an actual file on disk, but will be the name of the file from which the object was originally cloned, appended with an octothorpe (`#`) and the object instance number.

Object instance numbers start at 0 when the game is booted, and increase by one for each object cloned, hence the number is unique for each cloned object.
**ob** defaults to this_object() if not specified.
