# input_to()
## NAME
**input_to** - causes next line of input to be sent to a specified function

## SYNOPSIS
~~~cxx
varargs void input_to( string | function fun, int flag, ... );
~~~

## DESCRIPTION
Enable next line of user input to be sent to the local function **fun** as an argument.
The input line will not be parsed by the driver.

Note that `input_to` is non-blocking which means that the object calling `input_to` does not pause waiting for input.
Instead the object continues to execute any statements following the `input_to`.
The specified function **fun** will not be called until the user input has been collected.

If `input_to` is called more than once in the same execution, only the first call has any effect.

If optional argument **flag** is non-zero, the line given by the player will not be echoed, and is not seen if snooped (this is useful for collecting passwords).

The function **fun** will be called with the user input as its first argument (a string).
Any additional arguments supplied to `input_to` will be passed on to **fun** as arguments following the user input.

## IMPLEMENTATION NOTES
In Neolith, both string function names and function pointers are internally converted to local function pointers for efficient execution.
Carryover arguments are stored directly in the callback structure, ensuring clean memory management and supporting nested `input_to` calls.

## EXAMPLES
~~~cxx
// Basic usage
void prompt_name() {
    write("What is your name? ");
    input_to("receive_name", 0);
}

void receive_name(string name) {
    write("Hello, " + name + "!\n");
}

// With carryover arguments
void prompt_password(object user, string context) {
    write("Enter password: ");
    input_to("check_password", I_NOECHO, user, context);
}

void check_password(string password, object user, string context) {
    // password is first arg, user and context follow
    if (verify_password(user, password)) {
        write("Access granted for " + context + "\n");
    }
}

// Using function pointers
void setup_input() {
    input_to((: handle_input :), 0, "extra_data");
}

void handle_input(string input, string extra) {
    // Process input with extra context
}
~~~

## SEE ALSO
[call_other()](call_other.md),
[call_out()](call_out.md),
[get_char()](get_char.md)
