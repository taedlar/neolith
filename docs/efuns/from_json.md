# from_json()
## NAME
**from_json** - parse a JSON string into an LPC value

## SYNOPSIS
~~~cxx
mixed from_json( string json );
~~~

## DESCRIPTION
The `from_json()` efun parses a JSON string and returns the
corresponding LPC value.

This efun is available only when the driver is built with
`PACKAGE_JSON` enabled.

The conversion rules are:

- JSON integers become LPC ints.
- JSON floating-point numbers become LPC floats.
- JSON strings become LPC strings.
- JSON arrays become LPC arrays, recursively.
- JSON objects become LPC mappings with string keys.
- JSON `true` and `false` become LPC ints `1` and `0`.
- JSON `null` becomes the LPC undefined value.

Because JSON object keys are always strings, the mapping returned by
`from_json()` always uses string keys.

If `json` is not valid JSON text, `from_json()` raises a runtime error.

## SEE ALSO
[to_json()](to_json.md), [undefinedp()](undefinedp.md), [mapp()](mapp.md), [arrayp()](arrayp.md), [catch()](catch.md)