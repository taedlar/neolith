# moncontrol()
## NAME
**moncontrol** - turns on/off profiling during execution

## SYNOPSIS
~~~cxx
void moncontrol( int on );
~~~

## DESCRIPTION
If passed 1, moncontrol() enables profiling.  If passed 0,
moncontrol() disables profiling.  It can be called many
times during execution, typical use is to profile only
certain parts of driver execution.  moncontrol() has no
effect if profiling is not enabled at driver compile time.

## SEE ALSO
[opcprof()](opcprof.md), [function_profile()](function_profile.md)
