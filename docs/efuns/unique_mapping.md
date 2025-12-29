# unique_mapping()
## NAME
**unique_mapping** - create a mapping from an array based on a
function

## SYNOPSIS
~~~cxx
mapping unique_mapping( mixed *arr, string fun, object ob,
mixed extra, ... );
mapping unique_mapping( mixed *arr, function f, mixed extra, ... );
~~~

## DESCRIPTION
Returns a mapping built in the following manner:

members for which the function returns the same value are
grouped together, and associated with the return value as
the key.

## SEE ALSO
[filter_array()](filter_array.md), [sort_array()](sort_array.md), [map()](map.md)
