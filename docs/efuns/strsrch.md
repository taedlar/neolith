# strsrch
## NAME
          strsrch() - search for substrings in a string

## SYNOPSIS
          int strsrch( string str, string substr | int char,
                       int flag );

## DESCRIPTION
          strsrch() searches for the first occurance of the string
          `substr' in the string `str'.  The last occurance of
          `substr' can be found by passing `-1' as the 3rd argument
          (which is optional).  If the second argument is an integer,
          that character is found (a la C's strchr()/strrchr().)  The
          empty string or null value cannot be searched for.

## RETURN VALUE
          The integer offset of the first (last) match is returned.
          -1 is returned if there was no match, or an error occurred
          (bad args, etc).

## SEE ALSO
          explode(3), sscanf(3), replace_string(3), regexp(3)
