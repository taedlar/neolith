# get_char()
## NAME
**get_char** - causes next character of input to be sent to a specified function

## SYNOPSIS
~~~cxx
varargs void get_char( string | function fun, int flag, ...);
~~~

## DESCRIPTION
Enable next character of user input to be sent to the function **fun** as an argument.
The input character will not be parsed by the driver.

Note that `get_char` is non-blocking which means that the object calling `get_char` does not pause waiting for input.
Instead the object continues to execute any statements following the `get_char`.
The specified function **fun** will not be called until the user input has been collected.

If `get_char` is called more than once in the same execution, only the first call has any effect.

If optional argument **flag** is non-zero, the char given by the player will not be echoed, and is not seen if snooped (this is useful for collecting passwords).

The function **fun** will be called with the user input character as its first argument (a string).
Any additional arguments supplied to `get_char` will be passed on to **fun** as arguments following the user input.

In Neolith, the `get_char` is also supported in console mode by using termios to disable canonical mode.

## IMPLEMENTATION NOTES
In Neolith, `get_char` shares the same implementation architecture as `input_to`, using function pointers and unified callback argument handling.
Carryover arguments are stored efficiently in the callback structure.

## EXAMPLES
~~~cxx
// Single character menu
void show_menu() {
    write("Choose: [a]ttack [d]efend [r]un: ");
    get_char("handle_choice", 0);
}

void handle_choice(string ch) {
    switch(ch) {
        case "a": attack(); break;
        case "d": defend(); break;
        case "r": run_away(); break;
        default: write("Invalid choice\n"); show_menu();
    }
}

// With context arguments
void prompt_confirmation(object target, string action) {
    write("Confirm " + action + " on " + target->query_name() + "? [y/n]: ");
    get_char("confirm", 0, target, action);
}

void confirm(string ch, object target, string action) {
    if (ch == "y") {
        perform_action(target, action);
    } else {
        write("Cancelled.\n");
    }
}
~~~

## BUGS
Please note that get_char has a significant bug in MudOS 0.9 and earlier.
On many systems with poor telnet negotiation (read: almost every major workstation on the market), `get_char` makes screen output behave strangely.
It is not recommended for common usage throughout the mudlib until that bug is fixed.
(It is currently only known to work well for users connecting from NeXT computers.)

## SEE ALSO
[call_other()](call_other.md),
[call_out()](call_out.md),
[input_to()](input_to.md)
