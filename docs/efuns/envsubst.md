# envsubst()
## NAME
**envsubst** - expand environment variable references in a string

## SYNOPSIS
~~~cxx
string envsubst( string str );
~~~

## DESCRIPTION
Returns a copy of **str** with environment variable references replaced by
their values.  Three reference forms are supported:

| Form | Behavior |
|---|---|
| `$VAR` | Replaced by the value of `VAR`. |
| `${VAR}` | Replaced by the value of `VAR`. |
| `${VAR:-default}` | Replaced by the value of `VAR` if it is set and non-empty; otherwise replaced by `default`. |

Variable names must match the pattern `[A-Za-z_][A-Za-z0-9_]*`.

A `$` that is not followed by a valid variable name start character or `{` is
passed through as a literal `$`.  An unterminated `${` (no closing `}`) is
also passed through as a literal `$` and parsing continues from the `{`.

If a referenced variable is unset and no default is given, the reference
expands to an empty string.

If the fully expanded result would exceed the driver's internal buffer limit
(64 KiB), **str** is returned unchanged.

## EXAMPLE
~~~lpc
// Simple substitution
string token = envsubst("Bearer $API_TOKEN");

// With default fallback
string host = envsubst("${MUD_HOST:-localhost}");

// Constructing a URL from environment
string url = envsubst("http://${MUD_HOST:-localhost}:${MUD_PORT:-6000}/");
~~~

## SEE ALSO
[replace_string()](replace_string.md),
[c_str()](c_str.md)
