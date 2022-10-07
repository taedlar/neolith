# slow_shutdown
## NAME
          slow_shutdown - informs the mud that a slow shutdown is in
          progress

## SYNOPSIS
          void slow_shutdown( int minutes );

## DESCRIPTION
          This master apply is called when the driver can't allocate
          any more memory from the heap and had to use its reserved
          memory block.  This function can only be called if the
          "reserved size" config file setting was set.  The minutes
          remaining to driver shutdown is passed to this function.

## SEE ALSO
          crash(4), shutdown(3)
