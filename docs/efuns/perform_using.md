# perform_using()
## NAME
**perform_using** - configure a non-blocking HTTP request for the current object

## SYNOPSIS
~~~cxx
void perform_using(string opt, mixed val);
~~~

## DESCRIPTION
`perform_using()` stores request configuration on a per-object CURL handle. The
configuration persists across idle periods, so an object may set options once and
then call `perform_to()` repeatedly.

Only one configuration set is tracked per object. Options may not be changed while
that object already has a transfer in progress.

## SUPPORTED OPTIONS
- `"url"` with a string value sets the request URL.
- `"headers"` with a string or string array value sets request headers.
- `"post_data"` or `"body"` with a string or buffer value sets the request body and switches the transfer to HTTP POST.
- `"timeout_ms"` with a number value sets the libcurl timeout in milliseconds.
- `"follow_location"` with a non-zero number value enables redirect following.

Passing `0` clears `"url"`, `"headers"`, or `"post_data"`/`"body"`.

## ERRORS
An error is raised when:
- `opt` is not a string
- `val` has the wrong type for the selected option
- the current object already has an active `perform_to()` transfer
- the option name is unknown

## SEE ALSO
[perform_to()](perform_to.md), [in_perform()](in_perform.md)