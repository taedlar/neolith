# deep_inventory()
## NAME
**deep_inventory** - return the nested inventory of an object

## SYNOPSIS
~~~cxx
object *deep_inventory( object ob );
~~~

## DESCRIPTION
Returns an array of the objects contained in the inventory
of **ob** and all the objects contained in the inventories of
those objects and so on.

## SEE ALSO
[first_inventory()](first_inventory.md), [next_inventory()](next_inventory.md), [all_inventory()](all_inventory.md)
