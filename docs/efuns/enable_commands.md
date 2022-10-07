# enable_commands
## NAME
          enable_commands() - allow object to use 'player' commands

## SYNOPSIS
          void enable_commands( void );

## DESCRIPTION
          enable_commands() marks this_object() as a living object,
          and allows it to use commands added with add_action() (by
          using command()).  When enable_commands() is called, the
          driver also looks for the local function catch_tell(), and
          if found, it will call it every time a message (via say()
          for example) is given to the object.

## BUGS
          Do not call this function in any other place than create()
          or strange things will likely occur.

## SEE ALSO
          this_object(3), living(3), add_action(3), command(3),
          catch_tell(4), say(3), create(4)
