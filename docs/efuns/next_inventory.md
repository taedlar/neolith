# next_inventory()
## NAME
**next_inventory** - return the next object in the same
inventory

## SYNOPSIS
~~~cxx
object next_inventory( object ob );
~~~

## DESCRIPTION
Return the next object in the same inventory as `ob'.

Warning: If the object `ob' is moved by "move_object()",
then "next_inventory()" will return an object from the new
inventory.

## SEE ALSO
[first_inventory()](first_inventory.md), [all_inventory()](all_inventory.md), [deep_inventory()](deep_inventory.md)
