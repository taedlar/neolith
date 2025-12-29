# variables()
## NAME
**variables** - returns array of global variable names

## SYNOPSIS
~~~cxx
string* variables (object ob, int get_type default: 0);
~~~

## DESCRIPTION
If **get_type** is 0, returns array of global variable names.

If get_type is not 0, returns array of global variable name and type.

## SEE ALSO
[fetch_variable()](fetch_variable.md),
[store_variable()](store_variable.md)
