# get_save_file_name
## NAME
          get_save_file_name - back up editor file on abnormal exit

## SYNOPSIS
          string get_save_file_name( string );

## DESCRIPTION
          This master apply is called by ed() when a player
          disconnects while in the editor and editing a file.  This
          function should return an alternate file name for the file
          to be saved, to avoid overwriting the original.

     NOTE
          This apply used to be named get_ed_buffer_save_file_name().

## SEE ALSO
          ed(3)
