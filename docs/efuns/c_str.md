# c_str()

**c_str** - return the C-string prefix of an LPC string

## SYNOPSIS

```c
string c_str( string value );
```

## DESCRIPTION

`c_str()` makes the C-string boundary explicit.

It returns a new LPC string containing the bytes from `value` up to, but not
including, the first null byte (`\0`). If `value` contains no null byte, the
result is identical to `value`.

This efun is useful when passing LPC strings to APIs whose contract is based on
native C strings rather than counted byte spans. In particular, it can be used
to prepare URL, header, callback-name, or similar text values before handing
them to efuns or subsystems that ultimately depend on C-string semantics.

`c_str()` does not change the language-level meaning of LPC strings. LPC string
operators and counted-string storage remain byte-oriented and may contain
embedded null bytes.

## EXAMPLE

```c
string raw = "hello\0world";
string safe = c_str(raw);
// safe == "hello"
```

## SEE ALSO

[strcmp()](strcmp.md), [strlen()](strlen.md), [to_json()](to_json.md), [from_json()](from_json.md)