# remove_call_out
## NAME
          remove_call_out() - remove a pending call_out

## SYNOPSIS
          int remove_call_out( string fun | void );

## DESCRIPTION
          Remove next pending call out for function `fun' in the
          current object.  The return value is the time remaining
          before the callback is to be called.  The returned value is
          -1 if there were no call out pending to this function.

          不加參數時，會拿掉 this_object() 所有的 call_out。

## SEE ALSO
          call_out(3), call_out_info(3).
