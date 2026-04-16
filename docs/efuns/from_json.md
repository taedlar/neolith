# from_json()
## NAME
**from_json** - parse JSON text from a string or buffer into an LPC value

## SYNOPSIS
~~~cxx
mixed from_json( string | buffer json );
~~~

## DESCRIPTION
The `from_json()` efun parses JSON text and returns the
corresponding LPC value.

The input may be either an LPC string or an LPC buffer. Buffer input is
useful when JSON payload size can exceed the driver's max string length.

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

`from_json()` applies JSON text rules at this boundary:

- Input text must be valid JSON and valid UTF-8 where JSON requires it.
- Invalid UTF-8 and invalid Unicode escape sequences (including invalid
  surrogate usage) raise a runtime error.
- JSON escapes are decoded (`\\`, `\"`, control escapes, and `\uXXXX`
  sequences including surrogate pairs).
- Embedded null (U+0000, encoded as `\u0000`) is preserved as a byte in the
  resulting LPC string or mapping key; values are not truncated at C-string
  boundaries.

If `json` is not valid JSON text, `from_json()` raises a runtime error.

## SEE ALSO
[to_json()](to_json.md), [undefinedp()](undefinedp.md), [mapp()](mapp.md), [arrayp()](arrayp.md), [catch()](catch.md)