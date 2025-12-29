# rusage()
## NAME
**rusage** - reports information gathered by the getrusage()
system call

## SYNOPSIS
~~~cxx
mapping rusage( void );
~~~

## DESCRIPTION
This efun collects information gathered via the getrusage()
system call.  Read the getrusage() man page for more
information on what information will be collected.  Some
systems do not have the getrusage() system call but do have
the times() system call.  On those systems, only "utime" and
"stime" will be available.  Times are reported in
milliseconds.

Here is an example usage of rusage():

void
create()
{
mapping info;

info = rusage();
write("user time = " + info["utime"] + "ms\n");
write("system time = " + info["stime"] + "ms\n");
}

The available fields are:

utime, stime, maxrss, ixrss, idrss, isrss, minflt,
majflt, nswap, inblock, oublock, msgsnd, msgrcv,
nsignals, nvcsw, nivcsw.

## SEE ALSO
[time_expression()](time_expression.md), [function_profile()](function_profile.md), [time()](time.md), [uptime()](uptime.md)
