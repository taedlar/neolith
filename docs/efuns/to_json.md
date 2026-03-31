# to_json()
## NAME
**to_json** - serialize an LPC value to a JSON string

## SYNOPSIS
~~~cxx
string to_json( mixed value );
~~~

## DESCRIPTION
The `to_json()` efun converts an LPC value into a JSON string.

This efun is available only when the driver is built with
`PACKAGE_JSON` enabled.

The conversion rules are:

- int becomes a JSON number.
- float becomes a JSON number.
- string becomes a JSON string.
- array becomes a JSON array, recursively.
- mapping becomes a JSON object, recursively.
- undefined becomes JSON `null`.
- object, function, buffer, and class values become JSON `null`.

Mapping keys must be strings. If any mapping key is not a string,
`to_json()` raises a runtime error.

The exact spelling of floating-point numbers is not guaranteed.
Equivalent spellings such as `1.5` and `1.5E0` are both valid JSON.

## SEE ALSO
[from_json()](from_json.md), [mapp()](mapp.md), [arrayp()](arrayp.md), [undefinedp()](undefinedp.md), [catch()](catch.md)