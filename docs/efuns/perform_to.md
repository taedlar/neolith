# perform_to()
## NAME
**perform_to** - submit the configured non-blocking HTTP request for the current object

## SYNOPSIS
~~~cxx
void perform_to(string | function fun, int flag, mixed ...args);
~~~

## DESCRIPTION
`perform_to()` submits the current object's configured CURL request to the driver
worker thread and returns immediately. Completion callbacks are dispatched later on
the main thread.

At most one transfer may be active for a given object at a time.

The callback may be a function name string or a function pointer. On completion it
is invoked with:

~~~cxx
(int success, buffer | string body_or_error, mixed ...args)
~~~

`success` is non-zero when the request completed successfully.

- On success, `body_or_error` is a `buffer` containing the raw response body.
  An empty response body produces an empty buffer (size 0).
- On failure, `body_or_error` is a `string` containing the error message.

Any extra arguments passed to `perform_to()` are appended after those two standard
callback arguments.

If the owner object is destructed before completion, the transfer is cancelled and
the callback is not invoked.

## ERRORS
An error is raised when:
- `fun` is neither a string nor a function pointer
- `flag` is not a number
- no `"url"` has been configured with `perform_using()`
- the current object already has an active transfer
- the driver cannot queue the request

The `flag` argument is reserved for future use and is currently ignored beyond type
validation.

## SEE ALSO
[perform_using()](perform_using.md), [in_perform()](in_perform.md)